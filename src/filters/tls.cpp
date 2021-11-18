/*
 *  Copyright (c) 2019 by flomesh.io
 *
 *  Unless prior written consent has been obtained from the copyright
 *  owner, the following shall not be allowed.
 *
 *  1. The distribution of any source codes, header files, make files,
 *     or libraries of the software.
 *
 *  2. Disclosure of any source codes pertaining to the software to any
 *     additional parties.
 *
 *  3. Alteration or removal of any notices in or on the software or
 *     within the documentation included within the software.
 *
 *  ALL SOURCE CODE AS WELL AS ALL DOCUMENTATION INCLUDED WITH THIS
 *  SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION, WITHOUT WARRANTY OF ANY
 *  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "tls.hpp"
#include "context.hpp"
#include "module.hpp"
#include "pipeline.hpp"
#include "api/crypto.hpp"
#include "logging.hpp"

namespace pipy {
namespace tls {

static Data::Producer s_dp_tls("TLS");

static void throw_error() {
  char str[1000];
  auto err = ERR_get_error();
  ERR_error_string(err, str);
  throw std::runtime_error(str);
}

//
// TLSContext
//

TLSContext::TLSContext() {
  m_ctx = SSL_CTX_new(SSLv23_method());
  if (!m_ctx) throw_error();

  m_verify_store = X509_STORE_new();
  if (!m_verify_store) throw_error();

  SSL_CTX_set0_verify_cert_store(m_ctx, m_verify_store);
  SSL_CTX_set_tlsext_servername_callback(m_ctx, on_server_name);
}

TLSContext::~TLSContext() {
  if (m_ctx) SSL_CTX_free(m_ctx);
}

void TLSContext::add_certificate(crypto::Certificate *cert) {
  X509_STORE_add_cert(m_verify_store, cert->x509());
  SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
}

auto TLSContext::on_server_name(SSL *ssl, int*, void *thiz) -> int {
  TLSSession::get(ssl)->on_server_name();
  return SSL_TLSEXT_ERR_OK;
}

//
// TLSSession
//

int TLSSession::s_user_data_index = 0;

void TLSSession::init() {
  s_user_data_index = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
}

auto TLSSession::get(SSL *ssl) -> TLSSession* {
  auto ptr = SSL_get_ex_data(ssl, s_user_data_index);
  return reinterpret_cast<TLSSession*>(ptr);
}

TLSSession::TLSSession(
  TLSContext *ctx,
  Pipeline *pipeline,
  bool is_server,
  pjs::Object *certificate
)
  : m_peer_receiver(this)
  , m_pipeline(pipeline)
  , m_certificate(certificate)
  , m_is_server(is_server)
{
  m_ssl = SSL_new(ctx->ctx());
  SSL_set_ex_data(m_ssl, s_user_data_index, this);

  m_rbio = BIO_new(BIO_s_mem());
  m_wbio = BIO_new(BIO_s_mem());

  SSL_set_bio(m_ssl, m_rbio, m_wbio);

  pipeline->chain(m_peer_receiver.input());

  if (is_server) {
    SSL_set_accept_state(m_ssl);
    use_certificate(nullptr);

  } else {
    SSL_set_connect_state(m_ssl);
    if (m_certificate) use_certificate(nullptr);
  }
}

TLSSession::~TLSSession() {
  Pipeline::auto_release(m_pipeline);
  SSL_free(m_ssl);
}

void TLSSession::set_sni(const char *name) {
  SSL_set_tlsext_host_name(m_ssl, name);
}

void TLSSession::start_handshake() {
  do_handshake();
}

void TLSSession::on_event(Event *evt) {
  if (m_closed) return;

  if (auto *data = evt->as<Data>()) {
    if (m_is_server) {
      m_buffer_receive.push(*data);
      if (do_handshake()) pump_read();
    } else {
      m_buffer_send.push(*data);
      if (do_handshake()) pump_write();
    }

  } else if (evt->as<StreamEnd>()) {
    close();
  }
}

void TLSSession::on_receive_peer(Event *evt) {
  if (!m_closed) {
    if (auto *data = evt->as<Data>()) {
      if (m_is_server) {
        m_buffer_send.push(*data);
        if (do_handshake()) pump_write();
      } else {
        m_buffer_receive.push(*data);
        if (do_handshake()) pump_read();
      }
    } else if (auto end = evt->as<StreamEnd>()) {
      close(end);
    }
  }
}

void TLSSession::on_server_name() {
  if (auto name = SSL_get_servername(m_ssl, TLSEXT_NAMETYPE_host_name)) {
    pjs::Ref<pjs::Str> sni(pjs::Str::make(name));
    use_certificate(sni);
  }
}

void TLSSession::use_certificate(pjs::Str *sni) {
  pjs::Value certificate(m_certificate);
  if (certificate.is_function()) {
    Context &ctx = *m_pipeline->context();
    pjs::Value arg;
    if (sni) arg.set(sni);
    (*certificate.f())(ctx, 1, &arg, certificate);
    if (!ctx.ok()) return;
  }

  if (!certificate.is_object()) {
    Log::error("[tls] certificate callback did not return an object");
    return;
  }

  if (certificate.is_null()) return;

  pjs::Value cert, key;
  certificate.o()->get("cert", cert);
  certificate.o()->get("key", key);

  if (!key.is<crypto::PrivateKey>()) {
    Log::error("[tls] certificate.key requires a PrivateKey object");
    return;
  }

  SSL_use_PrivateKey(m_ssl, key.as<crypto::PrivateKey>()->pkey());

  if (cert.is<crypto::Certificate>()) {
    SSL_use_certificate(m_ssl, cert.as<crypto::Certificate>()->x509());
  } else if (cert.is<crypto::CertificateChain>()) {
    auto chain = cert.as<crypto::CertificateChain>();
    if (chain->size() < 1) {
      Log::error("[tls] empty certificate chain");
    } else {
      SSL_use_certificate(m_ssl, chain->x509(0));
      for (int i = 1; i < chain->size(); i++) {
        SSL_add1_chain_cert(m_ssl, chain->x509(i));
      }
    }
  } else {
    Log::error("[tls] certificate.cert requires a Certificate or a CertificateChain object");
  }
}

bool TLSSession::do_handshake() {
  while (!SSL_is_init_finished(m_ssl)) {
    auto ret = SSL_do_handshake(m_ssl);
    if (ret == 1) {
      pump_send();
      pump_write();
      return true;
    }
    auto status = SSL_get_error(m_ssl, ret);
    if (status != SSL_ERROR_WANT_READ && status != SSL_ERROR_WANT_WRITE) {
      Log::error("[tls] Handshake failed (error = %d)", status);
      while (auto err = ERR_get_error()) {
        char str[256];
        ERR_error_string(err, str);
        Log::error("[tls] %s", str);
      }
      close(StreamEnd::make(StreamEnd::UNAUTHORIZED));
      return false;
    }
    auto sent = pump_send();
    auto received = pump_receive();
    if (!received && !sent) return false;
  }
  return true;
}

auto TLSSession::pump_send() -> int {
  int size = 0;
  for (;;) {
    size_t n;
    pjs::Ref<Data> data = s_dp_tls.make(DATA_CHUNK_SIZE);
    auto chunk = data->chunks().begin();
    auto ptr = std::get<0>(*chunk);
    auto len = std::get<1>(*chunk);
    if (BIO_read_ex(m_wbio, ptr, len, &n)) {
      data->pop(data->size() - n);
      if (m_is_server) {
        output(data);
      } else {
        output(data, m_pipeline->input());
      }
      size += n;
    } else {
      break;
    }
  }
  if (size > 0) {
    if (m_is_server) {
      output(Data::flush());
    } else {
      output(Data::flush(), m_pipeline->input());
    }
  }
  return size;
}

auto TLSSession::pump_receive() -> int {
  int size = 0;
  for (const auto &c : m_buffer_receive.chunks()) {
    auto ptr = std::get<0>(c);
    auto len = std::get<1>(c);
    size_t n;
    if (!BIO_write_ex(m_rbio, ptr, len, &n)) break;
    size += n;
  }
  m_buffer_receive.shift(size);
  return size;
}

void TLSSession::pump_write() {
  int status = SSL_ERROR_NONE;
  while (m_buffer_send.size() > 0 && (
    status == SSL_ERROR_NONE ||
    status == SSL_ERROR_WANT_READ ||
    status == SSL_ERROR_WANT_WRITE
  )) {
    int size = 0;
    for (const auto &c : m_buffer_send.chunks()) {
      size_t n;
      auto ptr = std::get<0>(c);
      auto len = std::get<1>(c);
      auto ret = SSL_write_ex(m_ssl, ptr, len, &n);
      if (!ret) {
        status = SSL_get_error(m_ssl, ret);
        break;
      }
      size += n;
    }
    m_buffer_send.shift(size);
    pump_send();
    auto received = pump_receive();
    if (status == SSL_ERROR_WANT_READ && !received) break;
  }
}

void TLSSession::pump_read() {
  int size = 0;
  int status = SSL_ERROR_NONE;
  while (m_buffer_receive.size() > 0 && (
    status == SSL_ERROR_NONE ||
    status == SSL_ERROR_WANT_READ ||
    status == SSL_ERROR_WANT_WRITE
  )) {
    pump_send();
    auto received = pump_receive();
    if (status == SSL_ERROR_WANT_READ && !received) break;
    for (;;) {
      size_t n;
      pjs::Ref<Data> data = s_dp_tls.make(DATA_CHUNK_SIZE);
      auto chunk = data->chunks().begin();
      auto buf = std::get<0>(*chunk);
      auto len = std::get<1>(*chunk);
      auto ret = SSL_read_ex(m_ssl, buf, len, &n);
      if (!ret) {
        status = SSL_get_error(m_ssl, ret);
        break;
      } else {
        data->pop(data->size() - n);
        if (m_is_server) {
          output(data, m_pipeline->input());
        } else {
          output(data);
        }
        size += n;
      }
    }
  }
  if (size > 0) {
    if (m_is_server) {
      output(Data::flush(), m_pipeline->input());
    } else {
      output(Data::flush());
    }
  }
}

void TLSSession::close(StreamEnd *end) {
  if (!m_closed) {
    output(end ? end : StreamEnd::make());
    m_closed = true;
  }
}

//
// Client
//

Client::Client(pjs::Object *options)
  : m_tls_context(std::make_shared<TLSContext>())
{
  if (options) {
    pjs::Value certificate, trusted;
    options->get("certificate", certificate);
    options->get("trusted", trusted);
    options->get("sni", m_sni);

    if (!certificate.is_undefined()) {
      if (!certificate.is_object()) {
        throw std::runtime_error("options.certificate expects an object or a function");
      }
      m_certificate = certificate.o();
    }

    if (!trusted.is_undefined()) {
      if (!trusted.is_array()) {
        throw std::runtime_error("options.trusted expects an array");
      }
      trusted.as<pjs::Array>()->iterate_all([&](pjs::Value &v, int i) {
        if (!v.is<crypto::Certificate>()) {
          char msg[100];
          std::sprintf(msg, "element %d is not a Certificate", i);
          throw std::runtime_error(msg);
        }
        m_tls_context->add_certificate(v.as<crypto::Certificate>());
      });
    }

    if (!m_sni.is_undefined()) {
      if (!m_sni.is_string() && !m_sni.is_function()) {
        throw std::runtime_error("options.sni expects a string or a function");
      }
    }
  }
}

Client::Client(const Client &r)
  : Filter(r)
  , m_tls_context(r.m_tls_context)
  , m_certificate(r.m_certificate)
  , m_sni(r.m_sni)
{
}

Client::~Client() {
}

void Client::dump(std::ostream &out) {
  out << "connectTLS";
}

auto Client::clone() -> Filter* {
  return new Client(*this);
}

void Client::reset() {
  Filter::reset();
  delete m_session;
  m_session = nullptr;
}

void Client::process(Event *evt) {

  if (!m_session) {
    m_session = new TLSSession(
      m_tls_context.get(),
      sub_pipeline(0, false),
      false,
      m_certificate
    );
    m_session->chain(output());
    if (!m_sni.is_undefined()) {
      pjs::Value sni;
      if (!eval(m_sni, sni)) return;
      if (!sni.is_undefined()) {
        if (sni.is_string()) {
          m_session->set_sni(sni.s()->c_str());
        } else {
          Log::error("[tls] sni callback did not return a string");
        }
      }
    }
    m_session->start_handshake();
  }

  output(evt, m_session->input());
}

//
// Server
//

Server::Server(pjs::Object *options)
  : m_tls_context(std::make_shared<TLSContext>())
{
  if (options) {
    pjs::Value certificate, trusted;
    options->get("certificate", certificate);
    options->get("trusted", trusted);

    if (!certificate.is_undefined()) {
      if (!certificate.is_object()) {
        throw std::runtime_error("options.certificate expects an object or a function");
      }
      m_certificate = certificate.o();
    }

    if (!trusted.is_undefined()) {
      if (!trusted.is_array()) {
        throw std::runtime_error("options.trusted expects an array");
      }
      trusted.as<pjs::Array>()->iterate_all([&](pjs::Value &v, int i) {
        if (!v.is<crypto::Certificate>()) {
          char msg[100];
          std::sprintf(msg, "element %d is not a Certificate", i);
          throw std::runtime_error(msg);
        }
        m_tls_context->add_certificate(v.as<crypto::Certificate>());
      });
    }
  }
}

Server::Server(const Server &r)
  : Filter(r)
  , m_tls_context(r.m_tls_context)
  , m_certificate(r.m_certificate)
{
}

Server::~Server() {
}

void Server::dump(std::ostream &out) {
  out << "acceptTLS";
}

auto Server::clone() -> Filter* {
  return new Server(*this);
}

void Server::reset() {
  Filter::reset();
  delete m_session;
  m_session = nullptr;
}

void Server::process(Event *evt) {

  if (!m_session) {
    m_session = new TLSSession(
      m_tls_context.get(),
      sub_pipeline(0, false),
      true,
      m_certificate
    );
    m_session->chain(output());
  }

  output(evt, m_session->input());
}

} // namespace tls
} // namespace pipy
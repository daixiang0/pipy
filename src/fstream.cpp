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

#include "fstream.hpp"
#include "net.hpp"
#include "constants.hpp"
#include "pipeline.hpp"
#include "log.hpp"
#include "os-platform.hpp"

namespace pipy {

FileStream::FileStream(size_t read_size, os::FileHandle fd, Data::Producer *dp)
  : FlushTarget(true)
  , m_stream(Net::context(), fd.get())
  , m_fd(fd)
  , m_dp(dp)
  , m_read_size(read_size)
{
  read();
}

void FileStream::close() {
  if (m_closed) return;

  std::error_code ec;
  m_stream.release();
  if (!m_no_close) m_fd.close();

  if (m_receiving_state == PAUSED) {
    m_receiving_state = RECEIVING;
    release();
  }

  if (ec) {
    Log::error("FileStream: %p, error closing stream [fd = %d], %s", this, m_fd.get(), ec.message().c_str());
  } else if (Log::is_enabled(Log::FILES)) {
    Log::debug(Log::FILES, "FileStream: %p, stream closed [fd = %d]", this, m_fd.get());
  }

  m_closed = true;
}

void FileStream::on_event(Event *evt) {
  if (auto data = evt->as<Data>()) {
    write(data);
  } else if (evt->is<StreamEnd>()) {
    end();
  }
}

void FileStream::on_flush() {
  pump();
}

void FileStream::on_tap_open() {
  switch (m_receiving_state) {
    case PAUSING:
      m_receiving_state = RECEIVING;
      break;
    case PAUSED:
      m_receiving_state = RECEIVING;
      read();
      release();
      break;
    default: break;
  }
}

void FileStream::on_tap_close() {
  if (m_receiving_state == RECEIVING) {
    m_receiving_state = PAUSING;
  }
}

void FileStream::read() {
  if (!m_read_size) return;

  auto n = RECEIVE_BUFFER_SIZE;
  if (m_read_size > 0 && m_read_size < RECEIVE_BUFFER_SIZE) n = m_read_size;

  pjs::Ref<Data> buffer(m_dp->make(n));

  auto on_received = [=](const std::error_code &ec, size_t n) {
    InputContext ic(this);

    bool read_end = false;

    if (n > 0) {
      if (m_read_size > 0) {
        if (m_read_size > n) {
          m_read_size -= n;
        } else {
          n = m_read_size;
          m_read_size = 0;
          read_end = true;
        }
      }
      m_file_pointer += n;
      buffer->pop(buffer->size() - n);
      output(buffer);
    }

    if (read_end || ec == asio::error::eof || ec == asio::error::broken_pipe) {
      Log::debug(Log::FILES, "FileStream: %p, end of stream [fd = %d]", this, m_fd.get());
      output(StreamEnd::make(StreamEnd::NO_ERROR));
      close();
    } else if (ec && ec != asio::error::operation_aborted) {
      auto msg = ec.message();
      Log::warn(
        "FileStream: %p, error reading from stream [fd = %d]: %s",
        this, m_fd.get(), msg.c_str());
      output(StreamEnd::make(StreamEnd::READ_ERROR));
      close();

    } else if (m_receiving_state == PAUSING) {
      m_receiving_state = PAUSED;
      retain();

    } else if (m_receiving_state == RECEIVING) {
      read();
    }

    release();
  };

#ifdef _WIN32
  m_stream.async_read_some_at(
    m_file_pointer,
    DataChunks(buffer->chunks()),
    on_received
  );
#else
  m_stream.async_read_some(
    DataChunks(buffer->chunks()),
    on_received
  );
#endif

  retain();
}

void FileStream::write(Data *data) {
  if (!m_closed && !m_ended) {
    if (!data->empty()) {
      if (!m_overflowed) {
        if (m_buffer_limit > 0 && m_buffer.size() >= m_buffer_limit) {
          Log::error(
            "FileStream: %p, buffer overflow, size = %d, fd = %d",
            this, m_fd.get(), m_buffer.size());
          m_overflowed = true;
        }
      }

      if (!m_overflowed) {
        m_buffer.push(*data);
        need_flush();
      }
    }
  }
}

void FileStream::end() {
  if (!m_closed && !m_ended) {
    m_ended = true;
    if (m_buffer.empty()) {
      close();
    } else {
      pump();
    }
  }
}

void FileStream::pump() {
  if (m_pumping) return;
  if (m_buffer.empty()) return;

  auto on_sent = [=](const std::error_code &ec, std::size_t n) {
    m_file_pointer += n;
    m_buffer.shift(n);
    m_pumping = false;

    if (ec) {
      auto msg = ec.message();
      Log::warn(
        "FileStream: %p, error writing to stream [fd = %d], %s",
        this, m_fd.get(), msg.c_str());
      m_buffer.clear();

    } else {
      pump();
    }

    if (m_overflowed && m_buffer.size() < m_buffer_limit) {
      m_overflowed = false;
    }

    if (m_ended && m_buffer.empty()) close();

    release();
  };

#ifdef _WIN32
  m_stream.async_write_some_at(
    m_file_pointer,
    DataChunks(m_buffer.chunks()),
    on_sent
  );
#else
  m_stream.async_write_some(
    DataChunks(m_buffer.chunks()),
    on_sent
  );
#endif

  retain();

  m_pumping = true;
}

} // namespace pipy

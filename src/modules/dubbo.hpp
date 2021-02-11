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

#ifndef DUBBO_HPP
#define DUBBO_HPP

#include "module.hpp"
#include "buffer.hpp"

NS_BEGIN

namespace dubbo {

  //
  // Decoder
  //

  class Decoder : public Module {
  public:
    Decoder();

  private:
    ~Decoder();

    virtual auto help() -> std::list<std::string> override;
    virtual void config(const std::map<std::string, std::string> &params) override;
    virtual auto clone() -> Module* override;
    virtual void pipe(
      std::shared_ptr<Context> ctx,
      std::unique_ptr<Object> obj,
      Object::Receiver out
    ) override;

  private:
    enum State {
      FRAME_HEAD,
      FRAME_DATA,
    };

    State       m_state;
    int         m_size;
    ByteBuf<16> m_head;
    std::string m_var_request_id;
    std::string m_var_request_bit;
    std::string m_var_2_way_bit;
    std::string m_var_event_bit;
    std::string m_var_status;
  };

  //
  // Encoder
  //

  class Encoder : public Module {
  public:
    Encoder();

  private:
    ~Encoder();

    virtual auto help() -> std::list<std::string> override;
    virtual void config(const std::map<std::string, std::string> &params) override;
    virtual auto clone() -> Module* override;
    virtual void pipe(
      std::shared_ptr<Context> ctx,
      std::unique_ptr<Object> obj,
      Object::Receiver out
    ) override;

  private:
    std::unique_ptr<Data> m_buffer;
    std::string m_var_request_id;
    std::string m_var_request_bit;
    std::string m_var_2_way_bit;
    std::string m_var_event_bit;
    std::string m_var_status;
    long long m_auto_request_id = 0;

    static long long get_header(
      const Context &ctx,
      const std::string &expr,
      long long value
    );
  };

} // namespace dubbo

NS_END

#endif // DUBBO_HPP


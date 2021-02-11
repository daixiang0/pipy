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

#ifndef HESSIAN2_HPP
#define HESSIAN2_HPP

#include "module.hpp"

NS_BEGIN

namespace hessian2 {

  class Parser;

  //
  // Decoder
  //

  class Decoder : public Module {
  public:
    Decoder();

  private:
    ~Decoder();

    virtual auto help() -> std::list<std::string> override;
    virtual auto clone() -> Module* override;
    virtual void pipe(
      std::shared_ptr<Context> ctx,
      std::unique_ptr<Object> obj,
      Object::Receiver out
    ) override;

  private:
    bool    m_is_body;
    Parser* m_parser;
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
    virtual auto clone() -> Module* override;
    virtual void pipe(
      std::shared_ptr<Context> ctx,
      std::unique_ptr<Object> obj,
      Object::Receiver out
    ) override;

    Data m_buffer;
    bool m_is_body;
    int  m_level;

    void push(const std::string &str);
  };

} // namespace hessian2

NS_END

#endif // HESSIAN2_HPP


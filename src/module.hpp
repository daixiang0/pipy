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

#ifndef MODULE_HPP
#define MODULE_HPP

#include "ns.hpp"
#include "context.hpp"
#include "data.hpp"

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>

NS_BEGIN

//
// Module
//

class Module {
public:
  virtual ~Module() {}

  virtual auto help() -> std::list<std::string> { return std::list<std::string>(); }

  virtual void config(const std::map<std::string, std::string> &params) {}

  virtual auto clone() -> Module* = 0;

  virtual void pipe(
    std::shared_ptr<Context> ctx,
    std::unique_ptr<Object> obj,
    Object::Receiver out
  ) = 0;

protected:
  void set_timeout(double duration, std::function<void()> handler);
};

NS_END

#endif // MODULE_HPP

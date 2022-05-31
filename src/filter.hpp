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

#ifndef FILTER_HPP
#define FILTER_HPP

#include "event.hpp"
#include "list.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace pipy {

class Context;
class Module;
class PipelineDef;
class Pipeline;

//
// Filter
//

class Filter :
  public EventFunction,
  public List<Filter>::Item
{
public:
  virtual ~Filter() {}

  auto module() const -> Module*;
  auto context() const -> Context*;

  void add_sub_pipeline(PipelineDef *def);
  void add_sub_pipeline(pjs::Str *name);
  auto num_sub_pipelines() const -> int { return m_subs->size(); }
  auto get_sub_pipeline_name(int i) -> const std::string&;

  auto sub_pipeline(int i, bool clone_context) -> Pipeline*;

  virtual void bind();
  virtual auto clone() -> Filter* = 0;
  virtual void chain();
  virtual void reset();
  virtual void process(Event *evt) = 0;
  virtual void shutdown();
  virtual void dump(std::ostream &out) = 0;

protected:
  Filter();
  Filter(const Filter &r);

  using EventFunction::output;

  bool output(const pjs::Value &evt);
  bool callback(pjs::Function *func, int argc, pjs::Value argv[], pjs::Value &result);
  bool eval(pjs::Value &param, pjs::Value &result);
  bool eval(pjs::Function *func, pjs::Value &result);

private:
  struct Sub {
    pjs::Ref<pjs::Str> name;
    pjs::Ref<PipelineDef> def;
  };

  std::shared_ptr<std::vector<Sub>> m_subs;

  PipelineDef* m_pipeline_def = nullptr;
  Pipeline* m_pipeline = nullptr;
  bool m_stream_end = false;

  virtual void on_event(Event *evt) override;

  friend class Pipeline;
  friend class PipelineDef;
};

} // namespace pipy

#endif // FILTER_HPP

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

#ifndef PJS_BUILTIN_HPP
#define PJS_BUILTIN_HPP

#include "types.hpp"

#include <ctime>
#include <functional>

namespace pjs {

//
// Math
//

class Math : public ObjectTemplate<Math> {
};

//
// Date
//

class Date : public ObjectTemplate<Date> {
public:
  static auto now() -> double;

  auto getDate() -> int { return m_tm.tm_mday; }
  auto getDay() -> int { return m_tm.tm_wday; }
  auto getFullYear() -> int { return m_tm.tm_year + 1900; }
  auto getHours() -> int { return m_tm.tm_hour; }
  auto getMilliseconds() -> int { return m_msec; }
  auto getMinutes() -> int { return m_tm.tm_min; }
  auto getMonth() -> int { return m_tm.tm_mon; }
  auto getSeconds() -> int { return m_tm.tm_sec; }
  auto getTime() -> double;

  auto setDate(int value) -> double;
  auto setFullYear(int value) -> double;
  auto setFullYear(int y, int m) -> double;
  auto setFullYear(int y, int m, int d) -> double;
  auto setHours(int value) -> double;
  auto setHours(int h, int m) -> double;
  auto setHours(int h, int m, int s) -> double;
  auto setHours(int h, int m, int s, int ms) -> double;
  auto setMilliseconds(int value) -> double;
  auto setMinutes(int value) -> double;
  auto setMinutes(int m, int s) -> double;
  auto setMinutes(int m, int s, int ms) -> double;
  auto setMonth(int value) -> double;
  auto setMonth(int m, int d) -> double;
  auto setSeconds(int value) -> double;
  auto setSeconds(int s, int ms) -> double;
  auto setTime(double value) -> double;

  auto toDateString() -> std::string;
  auto toTimeString() -> std::string;
  auto toISOString() -> std::string;
  auto toUTCString() -> std::string;

  virtual void value_of(Value &out) override;
  virtual auto to_string() const -> std::string override;
  virtual auto dump() -> Object* override;

private:
  Date();
  Date(const Date *date);
  Date(double value);
  Date(int year, int mon, int day = 1, int hour = 0, int min = 0, int sec = 0, int ms = 0);

  std::tm m_tm;
  int m_msec;

  auto normalize() -> double;

  friend class ObjectTemplate<Date>;
};

template<>
class Constructor<Date> : public ConstructorTemplate<Date> {
public:
  void operator()(Context &ctx, Object *obj, Value &ret) {
  }
};

} // namespace pjs

#endif // PJS_BUILTIN_HPP

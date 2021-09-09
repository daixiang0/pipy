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

#ifndef CODEBASE_SERVICE_HPP
#define CODEBASE_SERVICE_HPP

#include "api/http.hpp"
#include "message.hpp"
#include "codebase-store.hpp"
#include "tar.hpp"

#include <map>
#include <set>

namespace pipy {

//
// CodebaseService
//

class CodebaseService {
public:
  CodebaseService(CodebaseStore *store);

  void open(int port);

private:
  CodebaseStore* m_store;

  Tarball m_www_files;
  std::map<std::string, pjs::Ref<http::File>> m_www_file_cache;

  pjs::Ref<http::ResponseHead> m_response_head_text;
  pjs::Ref<http::ResponseHead> m_response_head_json;
  pjs::Ref<Message> m_response_ok;
  pjs::Ref<Message> m_response_created;
  pjs::Ref<Message> m_response_not_found;
  pjs::Ref<Message> m_response_method_not_allowed;

  auto handle(Message *req) -> Message*;

  Message* repo_HEAD(const std::string &path);
  Message* repo_GET(const std::string &path);

  Message* api_v1_repo_GET(const std::string &path);
  Message* api_v1_repo_POST(const std::string &path, Data *data);
  Message* api_v1_repo_DELETE(const std::string &path);

  Message* api_v1_files_GET(const std::string &path);
  Message* api_v1_files_POST(const std::string &path, Data *data);

  Message* api_v1_program_GET();
  Message* api_v1_program_POST(Data *data);
  Message* api_v1_program_DELETE();

  Message* api_v1_config_GET();

  Message* api_v1_graph_POST(Data *data);

  Message* api_v1_log_GET(Message *req);

  Message* response(const Data &text);
  Message* response(const std::string &text);
  Message* response(const std::set<std::string> &list);
  Message* response(const pjs::Ref<pjs::Object> &json);
  Message* response(int status_code, const std::string &message);

  auto codebase_of(const std::string &path) -> CodebaseStore::Codebase*;

  static void file_tree_to_json(const std::string &path, std::string &json);

  static auto response_head(
    int status,
    const std::map<std::string, std::string> &headers
  ) -> http::ResponseHead*;
};

} // namespace pipy

#endif // CODEBASE_SERVICE_HPP
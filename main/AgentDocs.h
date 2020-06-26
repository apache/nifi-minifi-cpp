/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MAIN_AGENTDOCS_H_
#define MAIN_AGENTDOCS_H_

#include <iostream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace docs {

class AgentDocs {
 public:
  AgentDocs() = default;
  ~AgentDocs() = default;
  void generate(const std::string &docsdir, std::ostream &genStream);
 private:
  inline std::string extractClassName(const std::string &processor) const;
};

} /* namespace docs */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // MAIN_AGENTDOCS_H_

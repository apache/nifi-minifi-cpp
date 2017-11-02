/**
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

#ifndef NIFI_MINIFI_CPP_SCRIPTENGINE_H
#define NIFI_MINIFI_CPP_SCRIPTENGINE_H

#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace script {

class ScriptEngine {
 public:
  /**
   * Evaluates the given script string, storing the expression's result into res_var, if res_var is not empty.
   * @param script
   * @param res_var
   */
  virtual void eval(const std::string &script) = 0;

  /**
   * Evaluates the given script file, storing the expression's result into res_var, if res_var is not empty.
   * @param script
   * @param res_var
   */
  virtual void evalFile(const std::string &file_name) = 0;
};

} /* namespace script */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_SCRIPTENGINE_H

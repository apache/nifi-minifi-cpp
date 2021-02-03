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

#pragma once

#include "rapidjson/document.h"

#include "SQLWriter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

class JSONSQLWriter: public SQLWriter {
 public:
  explicit JSONSQLWriter(bool pretty);

  std::string toString() override;

private:
  void beginProcessRow() override;
  void endProcessRow() override;
  void processColumnNames(const std::vector<std::string>& name) override;
  void processColumn(const std::string& name, const std::string& value) override;
  void processColumn(const std::string& name, double value) override;
  void processColumn(const std::string& name, int value) override;
  void processColumn(const std::string& name, long long value) override;
  void processColumn(const std::string& name, unsigned long long value) override;
  void processColumn(const std::string& name, const char* value) override;

  void addToJSONRow(const std::string& columnName, rapidjson::Value&& jsonValue);

  rapidjson::Value toJSONString(const std::string& s);

 private:
  bool pretty_;
  rapidjson::Document jsonPayload_;
  rapidjson::Value jsonRow_;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */



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

#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

struct SQLRowSubscriber {
  virtual ~SQLRowSubscriber() = default;
  virtual void beginProcessBatch() = 0;
  virtual void endProcessBatch() = 0;
  virtual void beginProcessRow() = 0;
  virtual void endProcessRow() = 0;
  virtual void finishProcessing() = 0;
  virtual void processColumnNames(const std::vector<std::string>& names) = 0;
  virtual void processColumn(const std::string& name, const std::string& value) = 0;
  virtual void processColumn(const std::string& name, double value) = 0;
  virtual void processColumn(const std::string& name, int value) = 0;
  virtual void processColumn(const std::string& name, long long value) = 0;
  virtual void processColumn(const std::string& name, unsigned long long value) = 0;
  // Process NULL value.
  virtual void processColumn(const std::string& name, const char* value) = 0;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

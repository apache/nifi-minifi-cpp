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

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "minifi-cpp/core/Processor.h"
#include "minifi-cpp/core/ProcessorMetadata.h"

namespace org::apache::nifi::minifi::core {

class ProcessorFactory {
 public:
  virtual std::unique_ptr<ProcessorApi> create(ProcessorMetadata info) = 0;
  virtual std::string getGroupName() const = 0;
  virtual std::string getClassName() const = 0;

  virtual ~ProcessorFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core

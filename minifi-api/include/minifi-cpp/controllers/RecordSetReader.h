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


#include "core/controller/ControllerService.h"

#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "minifi-cpp/core/Record.h"
#include "utils/Enum.h"
#include "utils/ProcessorConfigUtils.h"


namespace org::apache::nifi::minifi::core {

class RecordSetReader : public virtual controller::ControllerService {
 public:
  virtual nonstd::expected<RecordSet, std::error_code> read(const std::shared_ptr<FlowFile>& flow_file, ProcessSession& session) = 0;
};

}  // namespace org::apache::nifi::minifi::core

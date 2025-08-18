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

#include <memory>

#include "core/Property.h"
#include "core/TypedValues.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core {

const  std::type_index DataSizeValue::type_id = typeid(uint64_t);
const  std::type_index DataTransferSpeedValue::type_id = typeid(uint64_t);
const  std::type_index TimePeriodValue::type_id = typeid(uint64_t);

std::shared_ptr<logging::Logger>& DataSizeValue::getLogger() {
  static std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<DataSizeValue>::getLogger();
  return logger;
}

}  // namespace org::apache::nifi::minifi::core

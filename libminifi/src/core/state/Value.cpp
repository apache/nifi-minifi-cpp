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

#include "core/state/Value.h"
#include <utility>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

const std::type_index Value::UINT64_TYPE = std::type_index(typeid(uint64_t));
const std::type_index Value::INT64_TYPE = std::type_index(typeid(int64_t));
const std::type_index Value::INT_TYPE = std::type_index(typeid(int));
const std::type_index Value::BOOL_TYPE = std::type_index(typeid(bool));
const std::type_index Value::STRING_TYPE = std::type_index(typeid(std::string));

} /* namespace response */
} /* namespace state */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


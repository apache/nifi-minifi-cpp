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

#include "utils/minifi-c-utils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::utils {

std::error_code make_error_code(MinifiStatus status) {
  switch (status) {
    case MINIFI_NOT_SUPPORTED_PROPERTY: return make_error_code(core::PropertyErrorCode::NotSupportedProperty);
    case MINIFI_DYNAMIC_PROPERTIES_NOT_SUPPORTED: return make_error_code(core::PropertyErrorCode::DynamicPropertiesNotSupported);
    case MINIFI_PROPERTY_NOT_SET: return make_error_code(core::PropertyErrorCode::PropertyNotSet);
    case MINIFI_VALIDATION_FAILED: return make_error_code(core::PropertyErrorCode::ValidationFailed);
    default: return std::error_code{};
  }
}

} //  namespace org::apache::nifi::minifi::cpp::utils
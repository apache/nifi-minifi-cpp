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

#include "core/PropertyValidation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

const std::shared_ptr<PropertyValidator> StandardValidators::VALID = std::make_shared<AlwaysValid>(true,"VALID");
const std::shared_ptr<PropertyValidator> StandardValidators::INVALID = std::make_shared<AlwaysValid>(false,"INVALID");
const std::shared_ptr<PropertyValidator> StandardValidators::INTEGER_VALIDATOR = std::make_shared<IntegerValidator>("INTEGER_VALIDATOR");
const std::shared_ptr<PropertyValidator> StandardValidators::LONG_VALIDATOR = std::make_shared<LongValidator>("LONG_VALIDATOR");
const std::shared_ptr<PropertyValidator> StandardValidators::SIZE_VALIDATOR = std::make_shared<DataSizeValidator>("DATA_SIZE_VALIDATOR");
const std::shared_ptr<PropertyValidator> StandardValidators::BOOLEAN_VALIDATOR = std::make_shared<BooleanValidator>("BOOLEAN_VALIDATOR");

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

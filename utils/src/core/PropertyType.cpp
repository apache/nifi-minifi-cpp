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

#include "core/PropertyType.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::core {

PropertyValue PropertyTypeImpl::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::ValueImpl>(input, *this);
}

PropertyValue BooleanPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::BoolValue>(input, *this);
}

PropertyValue IntegerPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::IntValue>(input, *this);
}

PropertyValue UnsignedIntPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::UInt32Value>(input, *this);
}

PropertyValue LongPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::Int64Value>(input, *this);
}

PropertyValue UnsignedLongPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<state::response::UInt64Value>(input, *this);
}

PropertyValue DataSizePropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<core::DataSizeValue>(input, *this);
}

PropertyValue TimePeriodPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<core::TimePeriodValue>(input, *this);
}

PropertyValue DataTransferSpeedPropertyType::parse(std::string_view input) const {
  return PropertyValue::parse<core::DataTransferSpeedValue>(input, *this);
}

namespace StandardPropertyTypes {

const core::PropertyType& translateCodeToPropertyType(const PropertyTypeCode& code) {
  switch (code) {
    case PropertyTypeCode::INTEGER:
      return core::StandardPropertyTypes::INTEGER_TYPE;
    case PropertyTypeCode::LONG:
      return core::StandardPropertyTypes::LONG_TYPE;
    case PropertyTypeCode::BOOLEAN:
      return core::StandardPropertyTypes::BOOLEAN_TYPE;
    case PropertyTypeCode::DATA_SIZE:
      return core::StandardPropertyTypes::DATA_SIZE_TYPE;
    case PropertyTypeCode::TIME_PERIOD:
      return core::StandardPropertyTypes::TIME_PERIOD_TYPE;
    case PropertyTypeCode::NON_BLANK:
      return core::StandardPropertyTypes::NON_BLANK_TYPE;
    case PropertyTypeCode::PORT:
      return core::StandardPropertyTypes::PORT_TYPE;
    default:
      throw std::invalid_argument("Unknown PropertyTypeCode");
  }
}

}  // namespace StandardPropertyTypes

}  // namespace org::apache::nifi::minifi::core

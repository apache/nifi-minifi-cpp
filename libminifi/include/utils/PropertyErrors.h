/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenseas/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBMINIFI_INCLUDE_UTILS_PROPERTYERRORS_H_
#define LIBMINIFI_INCLUDE_UTILS_PROPERTYERRORS_H_

#include <string>

#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

namespace core {

class PropertyValue;
class ConfigurableComponent;
class Property;

} /* namespace core */

namespace utils {
namespace internal {

class ValueException : public Exception {
 public:
  explicit ValueException(const std::string& err) : Exception(ExceptionType::GENERAL_EXCEPTION, err) {}
  explicit ValueException(const char* err) : Exception(ExceptionType::GENERAL_EXCEPTION, err) {}

  // base class already has a virtual destructor
};

class PropertyException : public Exception {
 public:
  explicit PropertyException(const std::string& err) : Exception(ExceptionType::GENERAL_EXCEPTION, err) {}
  explicit PropertyException(const char* err) : Exception(ExceptionType::GENERAL_EXCEPTION, err) {}

  // base class already has a virtual destructor
};

/**
 * Thrown during converting from and to Value
 */
class ConversionException : public ValueException {
  using ValueException::ValueException;
};

/**
 * Represents std::string -> Value conversion errors
 */
class ParseException : public ConversionException {
  using ConversionException::ConversionException;
};

/**
 * Thrown when trying to access invalid Values.
 */
class InvalidValueException : public ValueException {
  using ValueException::ValueException;
};

/**
 * When querying missing properties marked required.
 */
class RequiredPropertyMissingException : public PropertyException {
  using PropertyException::PropertyException;
};

} /* namespace internal */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_PROPERTYERRORS_H_

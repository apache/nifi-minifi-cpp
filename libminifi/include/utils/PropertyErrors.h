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

#ifndef NIFI_MINIFI_CPP_PROPERTYERRORS_H
#define NIFI_MINIFI_CPP_PROPERTYERRORS_H

#include "Exception.h"

namespace org{
namespace apache{
namespace nifi{
namespace minifi{

namespace core{

class PropertyValue;
class ConfigurableComponent;
class Property;

} /* namespace core */

namespace utils {

class ValueException: public Exception{
 private:
  ValueException(const std::string& err): Exception(ExceptionType::GENERAL_EXCEPTION, err) {}
  ValueException(const char* err): Exception(ExceptionType::GENERAL_EXCEPTION, err) {}

  // base class already has a virtual destructor

  friend class ParseException;
  friend class ConversionException;
  friend class InvalidValueException;
};

class PropertyException: public Exception{
 private:
  PropertyException(const std::string& err): Exception(ExceptionType::GENERAL_EXCEPTION, err) {}
  PropertyException(const char* err): Exception(ExceptionType::GENERAL_EXCEPTION, err) {}

  // base class already has a virtual destructor

  friend class RequiredPropertyMissingException;
};

/**
 * Thrown during converting from and to Value
 */
class ConversionException : public ValueException{
 private:
  ConversionException(const std::string& err): ValueException(err) {}
  ConversionException(const char* err): ValueException(err) {}

  friend class core::PropertyValue;
  friend class ParseException;
};

/**
 * Represents std::string -> Value conversion errors
 */
class ParseException : public ConversionException{
 private:
  ParseException(const std::string& err): ConversionException(err) {}
  ParseException(const char* err): ConversionException(err) {}

  friend class ValueParser;
};

/**
 * Thrown when trying to access invalid Values.
 */
class InvalidValueException : public ValueException{
 private:
  InvalidValueException(const std::string& err): ValueException(err) {}
  InvalidValueException(const char* err): ValueException(err) {}

  friend class core::PropertyValue;
  friend class core::Property;
};

/**
 * When querying missing properties marked required.
 */
class RequiredPropertyMissingException : public PropertyException{
 private:
  RequiredPropertyMissingException(const std::string& err): PropertyException(err) {}
  RequiredPropertyMissingException(const char* err): PropertyException(err) {}

  friend class core::ConfigurableComponent;
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PROPERTYERRORS_H

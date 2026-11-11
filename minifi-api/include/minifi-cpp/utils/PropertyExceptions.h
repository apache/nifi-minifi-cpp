// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>

#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi {

namespace core {

class PropertyValue;
class Property;

} /* namespace core */

namespace utils::internal {

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

}  // namespace utils::internal

}  // namespace org::apache::nifi::minifi

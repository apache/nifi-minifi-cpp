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

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <algorithm>

#ifndef NIFI_MINIFI_CPP_VALUE_H
#define NIFI_MINIFI_CPP_VALUE_H

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace expression {


/**
 * Represents an expression value, which can be one of multiple types or NULL
 */
class Value {
 public:

  /**
   * Construct a default (NULL) value
   */
  Value() {
  }

  /**
   * Construct a string value
   */
  explicit Value(std::string val) {
    setString(std::move(val));
  }

  /**
   * Construct a boolean value
   */
  explicit Value(bool val) {
    setBoolean(val);
  }

  /**
   * Construct an unsigned long value
   */
  explicit Value(uint64_t val) {
    setUnsignedLong(val);
  }

  /**
   * Construct a signed long value
   */
  explicit Value(int64_t val) {
    setSignedLong(val);
  }

  /**
   * Construct a long double value
   */
  explicit Value(long double val) {
    setLongDouble(val);
  }

  bool isNull() const {
    return is_null_;
  };

  bool isString() const {
    return is_string_;
  };

  bool isDecimal() const {
    if (is_long_double_) {
      return true;
    } else if (is_string_ && (string_val_.find('.') != string_val_.npos ||
        string_val_.find('e') != string_val_.npos ||
        string_val_.find('E') != string_val_.npos)) {
      return true;
    } else {
      return false;
    }
  }

  void setSignedLong(int64_t val) {
    is_null_ = false;
    is_bool_ = false;
    is_signed_long_ = true;
    is_unsigned_long_ = false;
    is_long_double_ = false;
    is_string_ = false;
    signed_long_val_ = val;
  }

  void setUnsignedLong(uint64_t val) {
    is_null_ = false;
    is_bool_ = false;
    is_signed_long_ = false;
    is_unsigned_long_ = true;
    is_long_double_ = false;
    is_string_ = false;
    unsigned_long_val_ = val;
  }

  void setLongDouble(long double val) {
    is_null_ = false;
    is_bool_ = false;
    is_signed_long_ = false;
    is_unsigned_long_ = false;
    is_long_double_ = true;
    is_string_ = false;
    long_double_val_ = val;
  }

  void setBoolean(bool val) {
    is_null_ = false;
    is_bool_ = true;
    is_signed_long_ = false;
    is_unsigned_long_ = false;
    is_long_double_ = false;
    is_string_ = false;
    bool_val_ = val;
  }

  void setString(std::string val) {
    is_null_ = false;
    is_bool_ = false;
    is_signed_long_ = false;
    is_unsigned_long_ = false;
    is_long_double_ = false;
    is_string_ = true;
    string_val_ = std::move(val);
  }

  std::string asString() const {
    if (is_string_) {
      return string_val_;
    } else if (is_bool_) {
      if (bool_val_) {
        return "true";
      } else {
        return "false";
      }
    } else if (is_signed_long_) {
      return std::to_string(signed_long_val_);
    } else if (is_unsigned_long_) {
      return std::to_string(unsigned_long_val_);
    } else if (is_long_double_) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(std::numeric_limits<double>::digits10)
         << long_double_val_;
      auto result = ss.str();
      result.erase(result.find_last_not_of('0') + 1, std::string::npos);

      if (result.find('.') == result.length() - 1) {
        result.erase(result.length() - 1, std::string::npos);
      }

      return result;
    } else {
      return "";
    }
  }

  uint64_t asUnsignedLong() const {
    if (is_unsigned_long_) {
      return unsigned_long_val_;
    } else if (is_string_) {
      return string_val_.empty() ? 0 : std::stoul(string_val_);
    } else if (is_signed_long_) {
      return signed_long_val_;
    } else if (is_long_double_) {
      return long_double_val_;
    } else {
      return 0.0;
    }
  }

  int64_t asSignedLong() const {
    if (is_signed_long_) {
      return signed_long_val_;
    } else if (is_unsigned_long_) {
      return unsigned_long_val_;
    } else if (is_string_) {
      return string_val_.empty() ? 0 : std::stol(string_val_);
    } else if (is_long_double_) {
      return long_double_val_;
    } else {
      return 0.0;
    }
  }

  long double asLongDouble() const {
    if (is_signed_long_) {
      return signed_long_val_;
    } else if (is_unsigned_long_) {
      return unsigned_long_val_;
    } else if (is_long_double_) {
      return long_double_val_;
    } else if (is_string_) {
      return string_val_.empty() ? 0 : std::stold(string_val_);
    } else {
      return 0.0;
    }
  }

  bool asBoolean() const {
    if (is_bool_) {
      return bool_val_;
    }
    if (is_signed_long_) {
      return signed_long_val_ != 0;
    } else if (is_unsigned_long_) {
      return unsigned_long_val_ != 0;
    } else if (is_long_double_) {
      return long_double_val_ != 0.0;
    } else if (is_string_) {
      std::string bool_str = string_val_;
      std::transform(bool_str.begin(), bool_str.end(), bool_str.begin(), ::tolower);
      std:: istringstream bools(bool_str);
      bool bool_val;
      bools >> std::boolalpha >> bool_val;
      return bool_val;
    } else {
      return false;
    }
  }

 private:
  bool is_null_ = true;
  bool is_string_ = false;
  bool is_bool_ = false;
  bool is_unsigned_long_ = false;
  bool is_signed_long_ = false;
  bool is_long_double_ = false;
  bool bool_val_ = false;
  uint64_t unsigned_long_val_ = 0;
  int64_t signed_long_val_ = 0;
  long double long_double_val_ = 0.0;
  std::string string_val_ = "";
};

} /* namespace expression */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_VALUE_H

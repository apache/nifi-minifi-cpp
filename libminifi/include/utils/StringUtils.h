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
#ifndef LIBMINIFI_INCLUDE_IO_STRINGUTILS_H_
#define LIBMINIFI_INCLUDE_IO_STRINGUTILS_H_

#include <algorithm>
#include <sstream>
#include "utils/FailurePolicy.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Stateless String utility class.
 *
 * Design: Static class, with no member variables
 *
 * Purpose: Houses many useful string utilities.
 */
class StringUtils {
 public:
  /**
   * Converts a string to a boolean
   * Better handles mixed case.
   * @param input input string
   * @param output output string.
   */
  static bool StringToBool(std::string input, bool &output) {

    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    std::istringstream(input) >> std::boolalpha >> output;
    return output;
  }

  // Trim String utils

  /**
   * Trims a string left to right
   * @param s incoming string
   * @returns modified string
   */
  static std::string trim(std::string s) {
    return trimRight(trimLeft(s));
  }

  /**
   * Trims left most part of a string
   * @param s incoming string
   * @returns modified string
   */
  static inline std::string trimLeft(std::string s) {
    s.erase(
        s.begin(),
        std::find_if(
            s.begin(), s.end(),
            std::not1(std::pointer_to_unary_function<int, int>(std::isspace))));
    return s;
  }

  /**
   * Trims a string on the right
   * @param s incoming string
   * @returns modified string
   */

  static inline std::string trimRight(std::string s) {
    s.erase(
        std::find_if(
            s.rbegin(), s.rend(),
            std::not1(std::pointer_to_unary_function<int, int>(std::isspace)))
            .base(),
        s.end());
    return s;
  }
  
  static std::vector<std::string> split(const std::string &str, const std::string &delimiter) {
    std::vector<std::string> result;
    int last = 0;
    int next = 0;
    while ((next = str.find(delimiter, last)) != std::string::npos) {
      result.push_back(str.substr(last, next - last));
      last = next + delimiter.length();
    }
    result.push_back(str.substr(last, next - last));
    return result;
  }
  
  static inline bool starts_with(const std::string &str, const std::string &prefix) {
    if (str.length() < prefix.length()) {
     return false;
    }
    return str.rfind(prefix, 0) == 0;
  }

  /**
   * Converts a string to a float
   * @param input input string
   * @param output output float
   * @param cp failure policy
   */
  static bool StringToFloat(std::string input, float &output, FailurePolicy cp =
                                RETURN) {
    try {
      output = std::stof(input);
    } catch (const std::invalid_argument &ie) {
      switch (cp) {
        case RETURN:
        case NOTHING:
          return false;
        case EXIT:
          exit(1);
        case EXCEPT:
          throw ie;
      }
    } catch (const std::out_of_range &ofr) {
      switch (cp) {
        case RETURN:
        case NOTHING:
          return false;
        case EXIT:
          exit(1);
        case EXCEPT:
          throw ofr;

      }
    }

    return true;

  }

};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_STRINGUTILS_H_ */

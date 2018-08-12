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
#include <iostream>
#include <functional>
#ifdef WIN32
	#include <cwctype>
	#include <cctype>
#endif
#include <algorithm>
#include <sstream>
#include <vector>
#include <map>
#include "utils/FailurePolicy.h"

enum TimeUnit {
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  MILLISECOND,
  NANOSECOND
};

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
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::pointer_to_unary_function<int, int>(isspace))));
    return s;
  }

  /**
   * Trims a string on the right
   * @param s incoming string
   * @returns modified string
   */

  static inline std::string trimRight(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::pointer_to_unary_function<int, int>(isspace))).base(), s.end());
    return s;
  }

  /**
   * Compares strings by lower casing them.
   */
  static inline bool equalsIgnoreCase(const std::string &left, const std::string right) {
    if (left.length() == right.length()) {
      return std::equal(right.begin(), right.end(), left.begin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
    } else {
      return false;
    }
  }

  static std::vector<std::string> split(const std::string &str, const std::string &delimiter) {
    std::vector<std::string> result;
    auto curr = str.begin();
    auto end = str.end();
    auto is_func = [delimiter](int s) {
      return delimiter.at(0) == s;
    };
    while (curr != end) {
      curr = std::find_if_not(curr, end, is_func);
      if (curr == end) {
        break;
      }
      auto next = std::find_if(curr, end, is_func);
      result.push_back(std::string(curr, next));
      curr = next;
    }

    return result;
  }

  /**
   * Converts a string to a float
   * @param input input string
   * @param output output float
   * @param cp failure policy
   */
  static bool StringToFloat(std::string input, float &output, FailurePolicy cp = RETURN) {
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

  static std::string replaceEnvironmentVariables(std::string& original_string) {
    int32_t beg_seq = 0;
    int32_t end_seq = 0;
    std::string source_string = original_string;
    do {
      beg_seq = source_string.find("${", beg_seq);
      if (beg_seq > 0 && source_string.at(beg_seq - 1) == '\\') {
        beg_seq += 2;
        continue;
      }
      if (beg_seq < 0)
        break;
      end_seq = source_string.find("}", beg_seq + 2);
      if (end_seq < 0)
        break;
      if (end_seq - (beg_seq + 2) < 0) {
        beg_seq += 2;
        continue;
      }
      const std::string env_field = source_string.substr(beg_seq + 2, end_seq - (beg_seq + 2));
      const std::string env_field_wrapped = source_string.substr(beg_seq, end_seq + 1);
      if (env_field.empty()) {
        continue;
      }
      const auto strVal = std::getenv(env_field.c_str());
      std::string env_value;
      if (strVal != nullptr)
        env_value = strVal;
      source_string = replaceAll(source_string, env_field_wrapped, env_value);
      beg_seq = 0;  // restart
    } while (beg_seq >= 0);

    source_string = replaceAll(source_string, "\\$", "$");

    return source_string;
  }

  static std::string& replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string) {
    std::size_t loc = 0;
    std::size_t lastFound;
    while ((lastFound = source_string.find(from_string, loc)) != std::string::npos) {
      source_string.replace(lastFound, from_string.size(), to_string);
      loc = lastFound + to_string.size();
    }
    return source_string;
  }

  inline static bool endsWithIgnoreCase(const std::string &value, const std::string & endString) {
    if (endString.size() > value.size())
      return false;
    return std::equal(endString.rbegin(), endString.rend(), value.rbegin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
  }

  inline static bool endsWith(const std::string &value, const std::string & endString) {
    if (endString.size() > value.size())
      return false;
    return std::equal(endString.rbegin(), endString.rend(), value.rbegin());
  }

  inline static std::string hex_ascii(const std::string& in) {
    int len = in.length();
    std::string newString;
    for (int i = 0; i < len; i += 2) {
      std::string sstr = in.substr(i, 2);
      char chr = (char) (int) strtol(sstr.c_str(), 0x00, 16);
      newString.push_back(chr);
    }
    return newString;
  }

  static std::string replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map) {
    auto result_string = source_string;

    std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;
    for (const auto &replace_pair : replace_map) {
      size_t replace_pos = 0;
      while ((replace_pos = source_string.find(replace_pair.first, replace_pos)) != std::string::npos) {
        replacements.emplace_back(std::make_pair(replace_pos, std::make_pair(replace_pair.first.length(), replace_pair.second)));
        replace_pos += replace_pair.first.length();
      }
    }

    std::sort(replacements.begin(), replacements.end(), [](const std::pair<size_t, std::pair<size_t, std::string>> a,
        const std::pair<size_t, std::pair<size_t, std::string>> &b) {
      return a.first > b.first;
    });

    for (const auto &replacement : replacements) {
      result_string = source_string.replace(replacement.first, replacement.second.first, replacement.second.second);
    }

    return result_string;
  }

};

} /* namespace utils */

namespace core{
enum TimeUnit {
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  MILLISECOND,
  NANOSECOND
};

} /* namespace core */

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_STRINGUTILS_H_ */

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
#if defined(WIN32) || (__cplusplus >= 201103L && (!defined(__GLIBCXX__) || (__cplusplus >= 201402L) ||  (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE > 4)))
#define HAVE_REGEX_CPP 1
#else
#define HAVE_REGEX_CPP 0
#endif
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

namespace {
  template<class Char>
  struct string_traits;
  template<>
  struct string_traits<char>{
    template<class T>
    static std::string convert_to_string(T&& t){
      return std::to_string(std::forward<T>(t));
    }
  };

  template<>
  struct string_traits<wchar_t>{
    template<class T>
    static std::wstring convert_to_string(T&& t){
      return std::to_wstring(std::forward<T>(t));
    }
  };
}

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
  static bool StringToBool(std::string input, bool &output);

  // Trim String utils

  /**
   * Trims a string left to right
   * @param s incoming string
   * @returns modified string
   */
  static std::string trim(std::string s);

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

  static std::vector<std::string> split(const std::string &str, const std::string &delimiter);

  /**
   * Converts a string to a float
   * @param input input string
   * @param output output float
   * @param cp failure policy
   */
  static bool StringToFloat(std::string input, float &output, FailurePolicy cp = RETURN);

  static std::string replaceEnvironmentVariables(std::string& original_string);

  static std::string& replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string);

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
  
  /**
   * Concatenates strings stored in an arbitrary container using the provided separator.
   * @tparam TChar char type of the string (char or wchar_t)
   * @tparam U arbitrary container which has string or wstring value type
   * @param separator that is inserted between each elements. Type should match the type of strings in container.
   * @param container that contains the strings to be concatenated
   * @return the result string
   */
  template<class TChar, class U, typename std::enable_if<std::is_same<typename U::value_type, std::basic_string<TChar>>::value>::type* = nullptr>
  static std::basic_string<TChar> join(const std::basic_string<TChar>& separator, const U& container) {
    typedef typename U::const_iterator ITtype;
    ITtype it = container.cbegin();
    std::basic_stringstream<TChar> sstream;
    while(it != container.cend()) {
      sstream << (*it);
      ++it;
      if(it != container.cend()) {
        sstream << separator;
      }
    }
    return sstream.str();
  };

  /**
   * Just a wrapper for the above function to be able to create separator from const char* or const wchar_t*
   */
  template<class TChar, class U, typename std::enable_if<std::is_same<typename U::value_type, std::basic_string<TChar>>::value>::type* = nullptr>
  static std::basic_string<TChar> join(const TChar* separator, const U& container) {
    return join(std::basic_string<TChar>(separator), container);
  };


  /**
   * Concatenates string representation of integrals stored in an arbitrary container using the provided separator.
   * @tparam TChar char type of the string (char or wchar_t)
   * @tparam U arbitrary container which has any integral value type
   * @param separator that is inserted between each elements. Type of this determines the result type. (wstring separator -> wstring)
   * @param container that contains the integrals to be concatenated
   * @return the result string
   */
  template<class TChar, class U, typename std::enable_if<std::is_integral<typename U::value_type>::value>::type* = nullptr,
      typename std::enable_if<!std::is_same<U, std::basic_string<TChar>>::value>::type* = nullptr>
  static std::basic_string<TChar> join(const std::basic_string<TChar>& separator, const U& container) {
    typedef typename U::const_iterator ITtype;
    ITtype it = container.cbegin();
    std::basic_stringstream<TChar> sstream;
    while(it != container.cend()) {
      sstream << string_traits<TChar>::convert_to_string(*it);
      ++it;
      if(it != container.cend()) {
        sstream << separator;
      }
    }
    return sstream.str();
  }

  /**
   * Just a wrapper for the above function to be able to create separator from const char* or const wchar_t*
   */
  template<class TChar, class U, typename std::enable_if<std::is_integral<typename U::value_type>::value>::type* = nullptr,
      typename std::enable_if<!std::is_same<U, std::basic_string<TChar>>::value>::type* = nullptr>
  static std::basic_string<TChar> join(const TChar* separator, const U& container) {
    return join(std::basic_string<TChar>(separator), container);
  };

  /**
   * Hexdecodes the hexencoded string in data, ignoring every character that is not [0-9a-fA-F]
   * @param data the output buffer where the hexdecoded bytes will be written. Must be at least length / 2 bytes long.
   * @param data_length pointer to the length of data the data buffer. It will be filled with the length of the decoded bytes.
   * @param hex the hexencoded string
   * @param hex_length the length of hex
   * @return true on success
   */
  inline static bool from_hex(uint8_t* data, size_t* data_length, const char* hex, size_t hex_length) {
    if (*data_length < hex_length / 2) {
      return false;
    }
    uint8_t n1;
    bool found_first_nibble = false;
    *data_length = 0;
    for (size_t i = 0; i < hex_length; i++) {
      const uint8_t byte = static_cast<uint8_t>(hex[i]);
      if (byte > 127) {
        continue;
      }
      uint8_t n = hex_lut[byte];
      if (n != SKIP) {
        if (found_first_nibble) {
          data[(*data_length)++] = n1 << 4 | n;
          found_first_nibble = false;
        } else {
          n1 = n;
          found_first_nibble = true;
        }
      }
    }
    if (found_first_nibble) {
      return false;
    }
    return true;
  }

  /**
   * Hexdecodes a string
   * @param hex the hexencoded string
   * @param hex_length the length of hex
   * @return the vector containing the hexdecoded bytes
   */
  inline static std::vector<uint8_t> from_hex(const char* hex, size_t hex_length) {
    std::vector<uint8_t> decoded(hex_length / 2);
    size_t data_length = decoded.size();
    if (!from_hex(decoded.data(), &data_length, hex, hex_length)) {
      throw std::runtime_error("Hexencoded string is malformatted");
    }
    decoded.resize(data_length);
    return decoded;
  }

  /**
   * Hexdecodes a string
   * @param hex the hexencoded string
   * @return the hexdecoded string
   */
  inline static std::string from_hex(const std::string& hex) {
    auto data = from_hex(hex.data(), hex.length());
    return std::string(reinterpret_cast<char*>(data.data()), data.size());
  }

  /**
   * Hexencodes bytes and writes the result to hex
   * @param hex the output buffer where the hexencoded string will be written. Must be at least length * 2 bytes long.
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max()
   * @param uppercase whether the hexencoded string should be upper case
   */
  inline static void to_hex(char* hex, const uint8_t* data, size_t length, bool uppercase) {
    for (size_t i = 0; i < length; i++) {
      hex[i * 2] = nibble_to_hex(data[i] >> 4, uppercase);
      hex[i * 2 + 1] = nibble_to_hex(data[i] & 0xf, uppercase);
    }
  }

  /**
   * Creates a hexencoded string from data
   * @param data the bytes to be hexencoded
   * @param length the length of the data
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  inline static std::string to_hex(const uint8_t* data, size_t length, bool uppercase = false) {
    if (length > (std::numeric_limits<size_t>::max)() / 2) {
      throw std::length_error("Data is too large to be hexencoded");
    }
    std::vector<char> buf(length * 2);
    to_hex(buf.data(), data, length, uppercase);
    return std::string(buf.data(), buf.size());
  }

  /**
   * Hexencodes a string
   * @param str the string to be hexencoded
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  inline static std::string to_hex(const std::string& str, bool uppercase = false) {
    return to_hex(reinterpret_cast<const uint8_t*>(str.data()), str.length(), uppercase);
  }

  static std::string replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map);

 private:
  inline static char nibble_to_hex(uint8_t nibble, bool uppercase) {
    if (nibble < 10) {
      return '0' + nibble;
    } else {
      return (uppercase ? 'A' : 'a') + nibble - 10;
    }
  }

  static constexpr uint8_t SKIP = 255;
  static constexpr uint8_t hex_lut[128] =
      {SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
       0x08, 0x09, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP,
       SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP, SKIP};

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

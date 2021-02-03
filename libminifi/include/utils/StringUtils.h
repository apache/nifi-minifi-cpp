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
#ifndef LIBMINIFI_INCLUDE_UTILS_STRINGUTILS_H_
#define LIBMINIFI_INCLUDE_UTILS_STRINGUTILS_H_

#include <string>
#include <utility>
#include <iostream>
#include <cstring>
#include <functional>
#ifdef WIN32
  #include <cwctype>
  #include <cctype>
#endif
#include <algorithm>
#include <sstream>
#include <vector>
#include <map>
#include <type_traits>
#include "utils/FailurePolicy.h"
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"
#include "utils/OptionalUtils.h"

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

  static utils::optional<bool> toBool(const std::string& input);

  static std::string toLower(std::string str);

  // Trim String utils

  /**
   * Trims a string left to right
   * @param s incoming string
   * @returns modified string
   */
  static std::string trim(const std::string& s);

  /**
   * Trims left most part of a string
   * @param s incoming string
   * @returns modified string
   */
  static inline std::string trimLeft(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) -> bool { return !isspace(c); }));
    return s;
  }

  /**
   * Trims a string on the right
   * @param s incoming string
   * @returns modified string
   */

  static inline std::string trimRight(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) -> bool { return !isspace(c); }).base(), s.end());
    return s;
  }

  /**
   * Compares strings by lower casing them.
   */
  static inline bool equalsIgnoreCase(const std::string& left, const std::string& right) {
    if (left.length() != right.length()) {
      return false;
    }
    return std::equal(right.cbegin(), right.cend(), left.cbegin(), [](unsigned char lc, unsigned char rc) { return std::tolower(lc) == std::tolower(rc); });
  }

  static std::vector<std::string> split(const std::string &str, const std::string &delimiter);
  static std::vector<std::string> splitAndTrim(const std::string &str, const std::string &delimiter);

  /**
   * Converts a string to a float
   * @param input input string
   * @param output output float
   * @param cp failure policy
   */
  static bool StringToFloat(std::string input, float &output, FailurePolicy cp = RETURN);

  static std::string replaceEnvironmentVariables(std::string source_string);

  static std::string replaceOne(const std::string &input, const std::string &from, const std::string &to);

  static std::string& replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string);

  inline static bool endsWithIgnoreCase(const std::string &value, const std::string & endString) {
    if (endString.size() > value.size())
      return false;
    return std::equal(endString.rbegin(), endString.rend(), value.rbegin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
  }

  inline static bool startsWith(const std::string& value, const std::string& start_string) {
    if (start_string.size() > value.size())
      return false;
    return std::equal(start_string.begin(), start_string.end(), value.begin());
  }

  inline static bool endsWith(const std::string& value, const std::string& end_string) {
    if (end_string.size() > value.size())
      return false;
    return std::equal(end_string.rbegin(), end_string.rend(), value.rbegin());
  }

  inline static std::string hex_ascii(const std::string& in) {
    std::string newString;
    newString.reserve(in.length() / 2);
    for (size_t i = 0; i < in.length(); i += 2) {
      std::string sstr = in.substr(i, 2);
      char chr = (char) (int) strtol(sstr.c_str(), 0x00, 16); // NOLINT
      newString.push_back(chr);
    }
    return newString;
  }

  /**
   * Returns the size of the passed std::w?string, C string or char array. Returns the array size - 1 in case of arrays.
   * @tparam CharT Character type, typically char or wchar_t (deduced)
   * @param str
   * @return The size of the argument string
   */
  template<typename CharT>
  static size_t size(const std::basic_string<CharT>& str) noexcept { return str.size(); }

  template<typename CharT>
  static size_t size(const CharT* str) noexcept { return std::char_traits<CharT>::length(str); }

  struct detail {
    // add all args
    template<typename... SizeT>
    static size_t sum(SizeT... ns) {
      size_t result = 0;
      (void)(std::initializer_list<size_t>{( result += ns )...});
      return result;  // (ns + ...)
    }

#ifndef _MSC_VER

    // partial detection idiom impl, from cppreference.com
    struct nonesuch{};

    template<typename Default, typename Void, template<class...> class Op, typename... Args>
    struct detector {
      using value_t = std::false_type;
      using type = Default;
    };

    template<typename Default, template<class...> class Op, typename... Args>
    struct detector<Default, void_t<Op<Args...>>, Op, Args...> {
      using value_t = std::true_type;
      using type = Op<Args...>;
    };

    template<template<class...> class Op, typename... Args>
    using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

    // and operation for boolean template argument packs
    template<bool...>
    struct and_;

    template<bool Head, bool... Tail>
    struct and_<Head, Tail...> : std::integral_constant<bool, Head && and_<Tail...>::value>
    {};

    template<bool B>
    struct and_<B> : std::integral_constant<bool, B>
    {};

    // implementation detail of join_pack
    template<typename CharT>
    struct str_detector {
      template<typename Str>
      using valid_string_t = decltype(std::declval<std::basic_string<CharT>>().append(std::declval<Str>()));
    };

    template<typename ResultT, typename CharT, typename... Strs>
    using valid_string_pack_t = typename std::enable_if<and_<is_detected<str_detector<CharT>::template valid_string_t, Strs>::value...>::value, ResultT>::type;
#else
    // MSVC is broken without /permissive-
  template<typename ResultT, typename...>
  using valid_string_pack_t = ResultT;
#endif

    template<typename CharT, typename... Strs, valid_string_pack_t<void, CharT, Strs...>* = nullptr>
    static std::basic_string<CharT> join_pack(const Strs&... strs) {
      std::basic_string<CharT> result;
      result.reserve(sum(size(strs)...));
      (void)(std::initializer_list<int>{( result.append(strs) , 0)...});
      return result;
    }
  }; /* struct detail */

  /**
   * Join all arguments
   * @tparam CharT Deduced character type
   * @tparam Strs Deduced string types
   * @param head First string, used for CharT deduction
   * @param tail Rest of the strings
   * @return std::basic_string<CharT> containing the resulting string
   */
  template<typename CharT, typename... Strs>
  static detail::valid_string_pack_t<std::basic_string<CharT>, CharT, Strs...>
  join_pack(const std::basic_string<CharT>& head, const Strs&... tail) {
    return detail::join_pack<CharT>(head, tail...);
  }

  template<typename CharT, typename... Strs>
  static detail::valid_string_pack_t<std::basic_string<CharT>, CharT, Strs...>
  join_pack(const CharT* head, const Strs&... tail) {
    return detail::join_pack<CharT>(head, tail...);
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
    while (it != container.cend()) {
      sstream << (*it);
      ++it;
      if (it != container.cend()) {
        sstream << separator;
      }
    }
    return sstream.str();
  }

  /**
   * Just a wrapper for the above function to be able to create separator from const char* or const wchar_t*
   */
  template<class TChar, class U, typename std::enable_if<std::is_same<typename U::value_type, std::basic_string<TChar>>::value>::type* = nullptr>
  static std::basic_string<TChar> join(const TChar* separator, const U& container) {
    return join(std::basic_string<TChar>(separator), container);
  }

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
    while (it != container.cend()) {
      sstream << string_traits<TChar>::convert_to_string(*it);
      ++it;
      if (it != container.cend()) {
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
  }

  /**
   * Hexdecodes a single character in the set [0-9a-fA-F]
   * @param ch the character to be decoded
   * @param output the reference where the result should be written
   * @return true on success
   */
  static bool from_hex(uint8_t ch, uint8_t& output);

  /**
   * Creates a string that is a concatenation of count instances of the provided string.
   * @tparam TChar char type of the string (char or wchar_t)
   * @param str that is to be repeated
   * @param count the number of times the string is repeated
   * @return the result string
   */
  template<typename TChar>
  static std::basic_string<TChar> repeat(const TChar* str, size_t count) {
    return repeat(std::basic_string<TChar>(str), count);
  }

  template<typename TChar>
  static std::basic_string<TChar> repeat(const std::basic_string<TChar>& str, size_t count) {
    return join("", std::vector<std::basic_string<TChar>>(count, str));
  }

  /**
   * Hexdecodes the hexencoded string in data, ignoring every character that is not [0-9a-fA-F]
   * @param data the output buffer where the hexdecoded bytes will be written. Must be at least length / 2 bytes long.
   * @param data_length pointer to the length of data the data buffer. It will be filled with the length of the decoded bytes.
   * @param hex the hexencoded string
   * @param hex_length the length of hex
   * @return true on success
   */
  static bool from_hex(uint8_t* data, size_t* data_length, const char* hex, size_t hex_length);

  /**
   * Hexdecodes a string
   * @param hex the hexencoded string
   * @param hex_length the length of hex
   * @return the vector containing the hexdecoded bytes
   */
  static std::vector<uint8_t> from_hex(const char* hex, size_t hex_length);

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
   * @param hex the output buffer where the hexencoded bytes will be written. Must be at least length * 2 bytes long.
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2
   * @param uppercase whether the hexencoded string should be upper case
   * @return the size of hexencoded bytes
   */
  static size_t to_hex(char* hex, const uint8_t* data, size_t length, bool uppercase);

  /**
   * Creates a hexencoded string from data
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2 - 1
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  static std::string to_hex(const uint8_t* data, size_t length, bool uppercase = false);

  /**
   * Hexencodes a string
   * @param str the string to be hexencoded
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  inline static std::string to_hex(const std::string& str, bool uppercase = false) {
    return to_hex(reinterpret_cast<const uint8_t*>(str.data()), str.length(), uppercase);
  }

  /**
   * Hexencodes a vector of bytes
   * @param data the vector of bytes to be hexencoded
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  inline static std::string to_hex(const std::vector<uint8_t>& data, bool uppercase = false) {
    return to_hex(data.data(), data.size(), uppercase);
  }

  /**
   * Decodes the Base64 encoded string into data
   * @param data the output buffer where the decoded bytes will be written. Must be at least (base64_length / 4 + 1) * 3 bytes long.
   * @param data_length pointer to the length of data the data buffer. It will be filled with the length of the decoded bytes.
   * @param base64 the Base64 encoded string
   * @param base64_length the length of base64
   * @return true on success
   */
  static bool from_base64(uint8_t* data, size_t* data_length, const char* base64, size_t base64_length);

  /**
   * Base64 decodes a string
   * @param base64 the Base64 encoded string
   * @param base64_length the length of base64
   * @return the vector containing the decoded bytes
   */
  static std::vector<uint8_t> from_base64(const char* base64, size_t base64_length);

  /**
   * Base64 decodes a string
   * @param base64 the Base64 encoded string
   * @return the decoded string
   */
  inline static std::string from_base64(const std::string& base64) {
    auto data = from_base64(base64.data(), base64.length());
    return std::string(reinterpret_cast<char*>(data.data()), data.size());
  }

  /**
   * Base64 encodes bytes and writes the result to base64
   * @param base64 the output buffer where the Base64 encoded bytes will be written. Must be at least (base64_length / 3 + 1) * 4 bytes long.
   * @param data the bytes to be Base64 encoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() * 3 / 4 - 3
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the size of Base64 encoded bytes
   */
  static size_t to_base64(char* base64, const uint8_t* data, size_t length, bool url, bool padded);

  /**
   * Creates a Base64 encoded string from data
   * @param data the bytes to be Base64 encoded
   * @param length the length of the data
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  static std::string to_base64(const uint8_t* data, size_t length, bool url = false, bool padded = true);

  /**
   * Base64 encodes a string
   * @param str the string to be Base64 encoded
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  inline static std::string to_base64(const std::string& str, bool url = false, bool padded = true) {
    return to_base64(reinterpret_cast<const uint8_t*>(str.data()), str.length(), url, padded);
  }

  /**
   * Base64 encodes a string
   * @param str the string to be Base64 encoded
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  inline static std::string to_base64(const std::vector<uint8_t>& str, bool url = false, bool padded = true) {
    return to_base64(str.data(), str.size(), url, padded);
  }

  static std::string replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map);

  static std::pair<size_t, int> countOccurrences(const std::string &str, const std::string &pattern) {
    if (pattern.empty()) {
      return {str.size(), gsl::narrow<int>(str.size() + 1)};
    }

    size_t last_pos = 0;
    int occurrences = 0;
    for (size_t pos = 0; (pos = str.find(pattern, pos)) != std::string::npos; pos += pattern.size()) {
      last_pos = pos;
      ++occurrences;
    }
    return {last_pos, occurrences};
  }

  /**
   * If the string starts and ends with the given character, the leading and trailing characters are removed.
   * @param str incoming string
   * @param c the character to be removed
   * @return the modified string if the framing character matches otherwise the incoming string
   */
  static std::string removeFramingCharacters(const std::string& str, char c) {
    if (str.size() < 2) {
      return str;
    }
    if (str[0] == c && str[str.size() - 1] == c) {
      return str.substr(1, str.size() - 2);
    }
    return str;
  }

 private:
  inline static char nibble_to_hex(uint8_t nibble, bool uppercase) {
    if (nibble < 10) {
      return '0' + nibble;
    } else {
      return (uppercase ? 'A' : 'a') + nibble - 10;
    }
  }

  inline static void base64_digits_to_bytes(const uint8_t digits[4], uint8_t* bytes) {
    bytes[0] = digits[0] << 2 | digits[1] >> 4;
    bytes[1] = (digits[1] & 0x0f) << 4 | digits[2] >> 2;
    bytes[2] = (digits[2] & 0x03) << 6 | digits[3];
  }

  static constexpr uint8_t SKIP = 0xff;
  static constexpr uint8_t ILGL = 0xfe;
  static constexpr uint8_t PDNG = 0xfd;
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

  static constexpr const char base64_enc_lut[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static constexpr const char base64_url_enc_lut[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  static constexpr uint8_t base64_dec_lut[128] =
      {ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL,
       ILGL, ILGL, SKIP, ILGL, ILGL, SKIP, ILGL, ILGL,
       ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL,
       ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL,
       ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL, ILGL,
       ILGL, ILGL, ILGL, 0x3e, ILGL, 0x3e, ILGL, 0x3f,
       0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
       0x3c, 0x3d, ILGL, ILGL, ILGL, PDNG, ILGL, ILGL,
       ILGL, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
       0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
       0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
       0x17, 0x18, 0x19, ILGL, ILGL, ILGL, ILGL, 0x3f,
       ILGL, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
       0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
       0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
       0x31, 0x32, 0x33, ILGL, ILGL, ILGL, ILGL, ILGL};
};

}  // namespace utils

namespace core {
enum TimeUnit {
  DAY,
  HOUR,
  MINUTE,
  SECOND,
  MILLISECOND,
  MICROSECOND,
  NANOSECOND
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_STRINGUTILS_H_

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

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef WIN32
  #include <cwctype>
  #include <cctype>
#endif
#include "utils/FailurePolicy.h"
#include "utils/gsl.h"
#include "utils/meta/detected.h"

namespace org::apache::nifi::minifi {
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

struct as_string_tag_t {};
inline constexpr as_string_tag_t as_string;

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
   * Checks and converts a string to a boolean
   * @param input input string
   * @returns an optional of a boolean: true if the string is "true" (ignoring case), false if it is "false" (ignoring case), nullopt for any other value
   */
  static std::optional<bool> toBool(const std::string& input);

  static std::string toLower(std::string_view str);

  /**
   * Strips the line ending (\n or \r\n) from the end of the input line.
   * @param input_line
   * @return (stripped line, line ending) pair
   */
  static std::pair<std::string, std::string> chomp(const std::string& input_line);

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

  static std::string_view trim(std::string_view sv);

  static std::string_view trim(const char* str);

  /**
   * Compares strings by lower casing them.
   */
  static inline bool equalsIgnoreCase(const std::string& left, const std::string& right) {
    if (left.length() != right.length()) {
      return false;
    }
    return std::equal(right.cbegin(), right.cend(), left.cbegin(), [](unsigned char lc, unsigned char rc) { return std::tolower(lc) == std::tolower(rc); });
  }

  static inline bool equals(const char* left, const char* right, bool caseSensitive = true) {
    if (caseSensitive) {
      return std::strcmp(left, right) == 0;
    }
    size_t left_len = std::strlen(left);
    size_t right_len = std::strlen(right);
    if (left_len != right_len) {
      return false;
    }
    return std::equal(right, right + right_len, left, [](unsigned char lc, unsigned char rc) { return std::tolower(lc) == std::tolower(rc); });
  }

  static inline bool equals(const std::string_view& left, const std::string_view& right, bool case_sensitive = true) {
    if (case_sensitive) {
      return left == right;
    }
    if (left.length() != right.length()) {
      return false;
    }
    return std::equal(left.begin(), left.end(), right.begin(), [](unsigned char lc, unsigned char rc) { return std::tolower(lc) == std::tolower(rc); });
  }

  static std::vector<std::string> split(const std::string &str, const std::string &delimiter);
  static std::vector<std::string> splitRemovingEmpty(const std::string& str, const std::string& delimiter);
  static std::vector<std::string> splitAndTrim(const std::string &str, const std::string &delimiter);
  static std::vector<std::string> splitAndTrimRemovingEmpty(const std::string& str, const std::string& delimiter);

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

  inline static bool startsWith(const std::string_view& value, const std::string_view& start, bool case_sensitive = true) {
    if (start.length() > value.length()) {
      return false;
    }
    if (case_sensitive) {
      return std::equal(start.begin(), start.end(), value.begin());
    }
    return std::equal(start.begin(), start.end(), value.begin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
  }

  inline static bool endsWith(const std::string_view& value, const std::string_view& end, bool case_sensitive = true) {
    if (end.length() > value.length()) {
      return false;
    }
    if (case_sensitive) {
      return std::equal(end.rbegin(), end.rend(), value.rbegin());
    }
    return std::equal(end.rbegin(), end.rend(), value.rbegin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
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

  template<typename CharT>
  static size_t size(const std::basic_string_view<CharT>& str) noexcept { return str.size(); }

  struct detail {
    // implementation detail of join_pack
    template<typename CharT>
    struct str_detector {
      template<typename Str>
      using valid_string_t = decltype(std::declval<std::basic_string<CharT>>().append(std::declval<Str>()));
    };

    template<typename ResultT, typename CharT, typename... Strs>
    using valid_string_pack_t = std::enable_if_t<(meta::is_detected_v<str_detector<CharT>::template valid_string_t, Strs> && ...), ResultT>;

    template<typename CharT, typename... Strs, valid_string_pack_t<void, CharT, Strs...>* = nullptr>
    static std::basic_string<CharT> join_pack(const Strs&... strs) {
      std::basic_string<CharT> result;
      result.reserve((size(strs) + ...));
      (result.append(strs), ...);
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

  template<typename CharT, typename... Strs>
  static detail::valid_string_pack_t<std::basic_string<CharT>, CharT, Strs...>
  join_pack(const std::basic_string_view<CharT>& head, const Strs&... tail) {
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
  static bool from_hex(std::byte* data, size_t* data_length, std::string_view hex);

  /**
   * Hexdecodes a string
   * @param hex the hexencoded string
   * @param hex_length the length of hex
   * @return the vector containing the hexdecoded bytes
   */
  static std::vector<std::byte> from_hex(std::string_view hex);
  static std::string from_hex(std::string_view hex, as_string_tag_t) {
    return utils::span_to<std::string>(gsl::make_span(from_hex(hex)).as_span<const char>());
  }


  /**
   * Hexencodes bytes and writes the result to hex
   * @param hex the output buffer where the hexencoded bytes will be written. Must be at least length * 2 bytes long.
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2
   * @param uppercase whether the hexencoded string should be upper case
   * @return the size of hexencoded bytes
   */
  static size_t to_hex(char* hex, gsl::span<const std::byte> data_to_be_transformed, bool uppercase);

  /**
   * Creates a hexencoded string from data
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2 - 1
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  static std::string to_hex(gsl::span<const std::byte> data_to_be_transformed, bool uppercase = false);

  /**
   * Hexencodes a string
   * @param str the string to be hexencoded
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  inline static std::string to_hex(std::string_view str, bool uppercase = false) {
    return to_hex(gsl::make_span(str).as_span<const std::byte>(), uppercase);
  }

  /**
   * Decodes the Base64 encoded string into data
   * @param data the output buffer where the decoded bytes will be written. Must be at least (base64_length / 4 + 1) * 3 bytes long.
   * @param data_length pointer to the length of data the data buffer. It will be filled with the length of the decoded bytes.
   * @param base64 the Base64 encoded string
   * @param base64_length the length of base64
   * @return true on success
   */
  static bool from_base64(std::byte* data, size_t* data_length, std::string_view base64);

  /**
   * Base64 decodes a string
   * @param base64 the Base64 encoded string
   * @param base64_length the length of base64
   * @return the vector containing the decoded bytes
   */
  static std::vector<std::byte> from_base64(std::string_view base64);
  static std::string from_base64(std::string_view base64, as_string_tag_t) {
    return utils::span_to<std::string>(gsl::make_span(from_base64(base64)).as_span<const char>());
  }

  /**
   * Base64 encodes bytes and writes the result to base64
   * @param base64 the output buffer where the Base64 encoded bytes will be written. Must be at least (base64_length / 3 + 1) * 4 bytes long.
   * @param raw_data the bytes to be Base64 encoded. Must not be larger than std::numeric_limits<size_t>::max() * 3 / 4 - 3
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the size of Base64 encoded bytes
   */
  static size_t to_base64(char* base64, gsl::span<const std::byte> data_to_be_transformed, bool url, bool padded);

  /**
   * Creates a Base64 encoded string from data
   * @param data the bytes to be Base64 encoded
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  static std::string to_base64(gsl::span<const std::byte> data_to_be_transformed, bool url = false, bool padded = true);

  /**
   * Base64 encodes a string
   * @param str the string to be Base64 encoded
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  inline static std::string to_base64(std::string_view str, bool url = false, bool padded = true) {
    return to_base64(gsl::make_span(str).as_span<const std::byte>(), url, padded);
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

  static std::string escapeUnprintableBytes(gsl::span<const std::byte> data);

 private:
};

}  // namespace utils
}  // namespace org::apache::nifi::minifi

#endif  // LIBMINIFI_INCLUDE_UTILS_STRINGUTILS_H_

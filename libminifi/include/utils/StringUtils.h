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
#pragma once

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
#include "utils/expected.h"
#include "utils/FailurePolicy.h"
#include "utils/gsl.h"
#include "utils/span.h"
#include "utils/meta/detected.h"
#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/join.hpp"

// libc++ doesn't define operator<=> on strings, and apparently the operator rewrite rules don't automagically make one
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 16000
#include <compare>

constexpr std::strong_ordering operator<=>(const std::string& lhs, const std::string& rhs) noexcept {
  return lhs.compare(rhs) <=> 0;
}
#endif

namespace org::apache::nifi::minifi::utils {

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

  static inline std::string toLower(std::string str) {
    const auto tolower = [](auto c) { return std::tolower(static_cast<unsigned char>(c)); };
    std::transform(std::begin(str), std::end(str), std::begin(str), tolower);
    return str;
  }

  static inline std::string toUpper(std::string str)  {
    const auto toupper = [](auto c) { return std::toupper(static_cast<unsigned char>(c)); };
    std::transform(std::begin(str), std::end(str), std::begin(str), toupper);
    return str;
  }

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
  static inline bool equalsIgnoreCase(std::string_view left, std::string_view right) {
    if (left.length() != right.length()) {
      return false;
    }
    return std::equal(right.cbegin(), right.cend(), left.cbegin(), [](unsigned char lc, unsigned char rc) { return std::tolower(lc) == std::tolower(rc); });
  }

  static constexpr bool equals(const char* left, const char* right, bool caseSensitive = true) {
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

  static constexpr bool equals(std::string_view left, std::string_view right, bool case_sensitive = true) {
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
  static bool StringToFloat(const std::string& input, float &output, FailurePolicy cp = RETURN);

  static std::string replaceEnvironmentVariables(std::string source_string);

  static std::string replaceOne(const std::string &input, const std::string &from, const std::string &to);

  static std::string& replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string);

  inline static bool startsWith(std::string_view value, std::string_view start, bool case_sensitive = true) {
    if (start.length() > value.length()) {
      return false;
    }
    if (case_sensitive) {
      return std::equal(start.begin(), start.end(), value.begin());
    }
    return std::equal(start.begin(), start.end(), value.begin(), [](unsigned char lc, unsigned char rc) {return tolower(lc) == tolower(rc);});
  }

  inline static bool endsWith(std::string_view value, std::string_view end, bool case_sensitive = true) {
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
   * Concatenates elements stored in a container to a string, using the separator.
   * @param separator that is inserted between the elements (no separator at the end)
   * @param container that contains the elements to be joined
   * @param projection function object which can convert an element to a string
   * @return the elements of the container (transformed by the projection) joined using the separator
   */
  template<typename Separator, typename Container, typename Projection>
  static auto join(Separator&& separator, Container&& container, Projection&& projection) {
    const auto separator_view = [&separator] {
      if constexpr (std::is_convertible_v<Separator, std::string_view>) { return std::string_view{separator}; }
      else if constexpr (std::is_convertible_v<Separator, std::wstring_view>) { return std::wstring_view{separator}; }
    }();

    return container
        | ranges::views::transform(projection)
        | ranges::views::join(separator_view)
        | ranges::to<std::basic_string>();
  }

  template<typename Separator, typename Container>
  requires(std::is_arithmetic_v<typename std::remove_reference_t<Container>::value_type> && std::is_convertible_v<Separator, std::string_view>)
  static auto join(Separator&& separator, Container&& container) {
    return join(separator, container, [](auto number) { return std::to_string(number); });
  }

  template<typename Separator, typename Container>
  requires(std::is_arithmetic_v<typename std::remove_reference_t<Container>::value_type> && std::is_convertible_v<Separator, std::wstring_view>)
  static auto join(Separator&& separator, Container&& container) {
    return join(separator, container, [](auto number) { return std::to_wstring(number); });
  }

  template<typename Separator, typename Container>
  static auto join(Separator&& separator, Container&& container) {
    return join(separator, container, [](auto x) { return x; });  // std::identity is not supported by AppleClang 13
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
    const auto decoded = from_hex(hex);
    return utils::span_to<std::string>(utils::as_span<const char>(std::span(decoded)));
  }


  /**
   * Hexencodes bytes and writes the result to hex
   * @param hex the output buffer where the hexencoded bytes will be written. Must be at least length * 2 bytes long.
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2
   * @param uppercase whether the hexencoded string should be upper case
   * @return the size of hexencoded bytes
   */
  static size_t to_hex(char* hex, std::span<const std::byte> data_to_be_transformed, bool uppercase);

  /**
   * Creates a hexencoded string from data
   * @param data the bytes to be hexencoded
   * @param length the length of data. Must not be larger than std::numeric_limits<size_t>::max() / 2 - 1
   * @param uppercase whether the hexencoded string should be upper case
   * @return the hexencoded string
   */
  static std::string to_hex(std::span<const std::byte> data_to_be_transformed, bool uppercase = false);

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
    const auto decoded = from_base64(base64);
    return utils::span_to<std::string>(utils::as_span<const char>(std::span(decoded)));
  }

  /**
   * Base64 encodes bytes and writes the result to base64
   * @param base64 the output buffer where the Base64 encoded bytes will be written. Must be at least (base64_length / 3 + 1) * 4 bytes long.
   * @param raw_data the bytes to be Base64 encoded. Must not be larger than std::numeric_limits<size_t>::max() * 3 / 4 - 3
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the size of Base64 encoded bytes
   */
  static size_t to_base64(char* base64, std::span<const std::byte> raw_data, bool url, bool padded);

  /**
   * Creates a Base64 encoded string from data
   * @param raw_data the bytes to be Base64 encoded
   * @param url if true, the URL-safe Base64 encoding will be used
   * @param padded if true, padding is added to the Base64 encoded string
   * @return the Base64 encoded string
   */
  static std::string to_base64(std::span<const std::byte> raw_data, bool url = false, bool padded = true);

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

  static std::string escapeUnprintableBytes(std::span<const std::byte> data);

  /**
   * Returns whether sequence of patterns are found in given string in their incoming order
   * Non-regex search!
   * @param str string to search in
   * @param patterns sequence of patterns to search
   * @return success of string sequence matching
   */
  static bool matchesSequence(std::string_view str, const std::vector<std::string>& patterns);

  static bool splitToValueAndUnit(std::string_view input, int64_t& value, std::string& unit);

  struct ParseError {};

  static nonstd::expected<std::optional<char>, ParseError> parseCharacter(const std::string &input);
};

}  // namespace org::apache::nifi::minifi::utils

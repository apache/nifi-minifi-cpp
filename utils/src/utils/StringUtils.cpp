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

#include <limits>
#include <charconv>

#include "utils/Environment.h"
#include "utils/GeneralUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils::string {

std::optional<bool> toBool(const std::string& input) {
  std::string trimmed = trim(input);
  if (equalsIgnoreCase(trimmed, "true")) {
    return true;
  }
  if (equalsIgnoreCase(trimmed, "false")) {
    return false;
  }
  return std::nullopt;
}

std::pair<std::string, std::string> chomp(const std::string& input_line) {
  if (endsWith(input_line, "\r\n")) {
    return std::make_pair(input_line.substr(0, input_line.size() - 2), "\r\n");
  } else if (endsWith(input_line, "\n")) {
    return std::make_pair(input_line.substr(0, input_line.size() - 1), "\n");
  } else {
    return std::make_pair(input_line, "");
  }
}

std::string trim(const std::string& s) {
  return trimRight(trimLeft(s));
}

std::string_view trim(std::string_view sv) {
  auto begin = std::find_if(sv.begin(), sv.end(), [](unsigned char c) -> bool { return !isspace(c); });
  auto end = std::find_if(sv.rbegin(), std::reverse_iterator(begin), [](unsigned char c) -> bool { return !isspace(c); }).base();
  // c++20 iterator constructor
  // return std::string_view(begin, end);
  // but for now
  // on windows std::string_view::const_iterator is not a const char*
  return sv.substr(std::distance(sv.begin(), begin), std::distance(begin, end));
}

std::string_view trim(const char* str) {
  return trim(std::string_view(str));
}

template<typename Fun>
std::vector<std::string> split_transformed(std::string_view str_view, std::string_view delimiter, Fun transformation) {
  std::string str{str_view};
  std::vector<std::string> result;
  if (delimiter.empty()) {
    for (auto c : str) {
      result.push_back(transformation(std::string(1, c)));
    }
    return result;
  }

  size_t pos = str.find(delimiter);
  if (pos == std::string::npos) {
    result.push_back(transformation(str));
    return result;
  }
  while (pos != std::string::npos) {
    result.push_back(transformation(str.substr(0, pos)));
    str = str.substr(pos + delimiter.size());
    pos = str.find(delimiter);
  }
  result.push_back(transformation(str));
  return result;
}

std::vector<std::string> split(std::string_view str, std::string_view delimiter) {
  return split_transformed(str, delimiter, identity{});
}

std::vector<std::string> splitRemovingEmpty(std::string_view str, std::string_view delimiter) {
  auto result = split(str, delimiter);
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string& str) { return str.empty(); }), result.end());
  return result;
}

std::vector<std::string> splitAndTrim(std::string_view str, std::string_view delimiter) {
  return split_transformed(str, delimiter, static_cast<std::string(*)(const std::string&)>(trim));
}

std::vector<std::string> splitAndTrimRemovingEmpty(std::string_view str, std::string_view delimiter) {
  auto result = splitAndTrim(str, delimiter);
  result.erase(std::remove_if(result.begin(), result.end(), [](const std::string& str) { return str.empty(); }), result.end());
  return result;
}

bool StringToFloat(const std::string& input, float &output, FailurePolicy cp /*= RETURN*/) {
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

std::string replaceEnvironmentVariables(std::string source_string) {
  std::string::size_type beg_seq = 0;
  std::string::size_type end_seq = 0;
  do {
    beg_seq = source_string.find("${", beg_seq);
    if (beg_seq == std::string::npos) {
      break;
    }
    if (beg_seq > 0 && source_string.at(beg_seq - 1) == '\\') {
      beg_seq += 2;
      continue;
    }
    end_seq = source_string.find('}', beg_seq + 2);
    if (end_seq == std::string::npos) {
      break;
    }
    if (end_seq <= beg_seq + 2) {
      beg_seq += 2;
      continue;
    }
    auto env_var_length = end_seq - (beg_seq + 2);
    const std::string env_var = source_string.substr(beg_seq + 2, env_var_length);
    const std::string env_var_wrapped = source_string.substr(beg_seq, env_var_length + 3);

    auto env_value = utils::Environment::getEnvironmentVariable(env_var.c_str()).value_or("");

    source_string = replaceAll(source_string, env_var_wrapped, env_value);
    beg_seq = 0;  // restart
  } while (beg_seq < source_string.size());

  source_string = replaceAll(source_string, "\\$", "$");

  return source_string;
}

std::string replaceOne(const std::string &input, const std::string &from, const std::string &to) {
  std::size_t found_at_position = input.find(from);
  if (found_at_position != std::string::npos) {
    std::string input_copy = input;
    return input_copy.replace(found_at_position, from.size(), to);
  } else {
    return input;
  }
}

std::string& replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string) {
  std::size_t loc = 0;
  std::size_t lastFound;
  while ((lastFound = source_string.find(from_string, loc)) != std::string::npos) {
    source_string.replace(lastFound, from_string.size(), to_string);
    loc = lastFound + to_string.size();
    if (from_string.empty()) {
      loc++;
    }
  }
  return source_string;
}

std::string replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map) {
  auto result_string = source_string;

  std::vector<std::pair<size_t, std::pair<size_t, std::string>>> replacements;
  for (const auto &replace_pair : replace_map) {
    size_t replace_pos = 0;
    while ((replace_pos = source_string.find(replace_pair.first, replace_pos)) != std::string::npos) {
      replacements.emplace_back(std::make_pair(replace_pos, std::make_pair(replace_pair.first.length(), replace_pair.second)));
      replace_pos += replace_pair.first.length();
    }
  }

  std::sort(replacements.begin(), replacements.end(), [](const std::pair<size_t, std::pair<size_t, std::string>> &a,
                                                         const std::pair<size_t, std::pair<size_t, std::string>> &b) {
    return a.first > b.first;
  });

  for (const auto &replacement : replacements) {
    result_string = source_string.replace(replacement.first, replacement.second.first, replacement.second.second);
  }

  return result_string;
}

namespace {
char nibble_to_hex(uint8_t nibble, bool uppercase) {
  if (nibble < 10) {
    return '0' + nibble;
  } else {
    return (uppercase ? 'A' : 'a') + nibble - 10;
  }
}

void base64_digits_to_bytes(const uint8_t digits[4], std::byte* const bytes) {
  bytes[0] = static_cast<std::byte>(digits[0] << 2 | digits[1] >> 4);
  bytes[1] = static_cast<std::byte>((digits[1] & 0x0f) << 4 | digits[2] >> 2);
  bytes[2] = static_cast<std::byte>((digits[2] & 0x03) << 6 | digits[3]);
}

constexpr uint8_t SKIP = 0xff;
constexpr uint8_t ILGL = 0xfe;
constexpr uint8_t PDNG = 0xfd;
constexpr uint8_t hex_lut[128] =
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

constexpr const char base64_enc_lut[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr const char base64_url_enc_lut[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr uint8_t base64_dec_lut[128] =
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
}  // namespace

bool from_hex(uint8_t ch, uint8_t& output) {
  if (ch > 127) {
    return false;
  }
  output = hex_lut[ch];
  return output != SKIP;
}

bool from_hex(std::byte* data, size_t* data_length, std::string_view hex) {
  if (*data_length < hex.size() / 2) {
    return false;
  }
  uint8_t n1;
  bool found_first_nibble = false;
  *data_length = 0;
  for (char c : hex) {
    const auto byte = static_cast<uint8_t>(c);
    if (byte > 127) {
      continue;
    }
    uint8_t n = hex_lut[byte];
    if (n != SKIP) {
      if (found_first_nibble) {
        data[(*data_length)++] = static_cast<std::byte>(n1 << 4 | n);
        found_first_nibble = false;
      } else {
        n1 = n;
        found_first_nibble = true;
      }
    }
  }
  return !found_first_nibble;
}

std::vector<std::byte> from_hex(std::string_view hex) {
  std::vector<std::byte> decoded(hex.size() / 2);
  size_t data_length = decoded.size();
  if (!from_hex(decoded.data(), &data_length, hex)) {
    throw std::invalid_argument("Hexencoded string is malformed");
  }
  decoded.resize(data_length);
  return decoded;
}

size_t to_hex(char* hex, std::span<const std::byte> data_to_be_transformed, bool uppercase) {
  if (data_to_be_transformed.size() > std::numeric_limits<size_t>::max() / 2) {
    throw std::length_error("Data is too large to be hexencoded");
  }
  for (size_t i = 0; i < data_to_be_transformed.size(); i++) {
    hex[i * 2] = nibble_to_hex(static_cast<uint8_t>(data_to_be_transformed[i]) >> 4, uppercase);
    hex[i * 2 + 1] = nibble_to_hex(static_cast<uint8_t>(data_to_be_transformed[i]) & 0xf, uppercase);
  }
  return data_to_be_transformed.size() * 2;
}

std::string to_hex(std::span<const std::byte> data_to_be_transformed, bool uppercase /*= false*/) {
  if (data_to_be_transformed.size() > (std::numeric_limits<size_t>::max() / 2 - 1)) {
    throw std::length_error("Data is too large to be hexencoded");
  }
  std::string result;
  result.resize(data_to_be_transformed.size() * 2);
  const size_t hex_length = to_hex(result.data(), data_to_be_transformed, uppercase);
  gsl_Assert(hex_length == result.size());
  return result;
}

bool from_base64(std::byte* const data, size_t* const data_length, const std::string_view base64) {
  if (*data_length < (base64.size() / 4 + 1) * 3) {
    return false;
  }

  uint8_t digits[4];
  size_t digit_counter = 0U;
  size_t decoded_size = 0U;
  size_t padding_counter = 0U;
  size_t i;
  for (i = 0U; i < base64.size(); i++) {
    const auto byte = static_cast<uint8_t>(base64[i]);
    if (byte > 127) {
      return false;
    }

    const uint8_t decoded = base64_dec_lut[byte];
    switch (decoded) {
      case SKIP:
        continue;
      case ILGL:
        return false;
      case PDNG:
        padding_counter++;
        continue;
      default:
        if (padding_counter > 0U) {
          return false;
        }
        digits[digit_counter++] = decoded;
        if (digit_counter == 4U) {
          base64_digits_to_bytes(digits, data + decoded_size);
          decoded_size += 3U;
          digit_counter = 0U;
        }
    }
  }

  if (padding_counter > 0U && padding_counter != 4U - digit_counter) {
    return false;
  }

  switch (digit_counter) {
    case 0:
      break;
    case 1:
      return false;
    case 2:
      digits[2] = 0x00;
      [[fallthrough]];
    case 3: {
      digits[3] = 0x00;

      std::byte bytes_temp[3];
      base64_digits_to_bytes(digits, bytes_temp);
      const size_t num_bytes = digit_counter - 1;
      memcpy(data + decoded_size, bytes_temp, num_bytes);
      decoded_size += num_bytes;
      break;
    }
    default:
      return false;
  }

  *data_length = decoded_size;
  return true;
}

std::vector<std::byte> from_base64(const std::string_view base64) {
  std::vector<std::byte> decoded((base64.size() / 4 + 1) * 3);
  size_t data_length = decoded.size();
  if (!from_base64(decoded.data(), &data_length, base64)) {
    throw std::invalid_argument("Base64 encoded string is malformed");
  }
  decoded.resize(data_length);
  return decoded;
}

size_t to_base64(char* base64, const std::span<const std::byte> raw_data, bool url, bool padded) {
  gsl_Expects(base64);
  if (raw_data.size() > std::numeric_limits<size_t>::max() * 3 / 4 - 3) {
    throw std::length_error("Data is too large to be base64 encoded");
  }
  constexpr auto null_byte = static_cast<std::byte>(0x00);

  const char* enc_lut = url ? base64_url_enc_lut : base64_enc_lut;
  size_t base64_length = 0U;
  std::byte bytes[3];
  for (size_t i = 0U; i < raw_data.size(); i += 3U) {
    const bool b1_present = i + 1 < raw_data.size();
    const bool b2_present = i + 2 < raw_data.size();
    bytes[0] = raw_data[i];
    bytes[1] = b1_present ? raw_data[i + 1] : null_byte;
    bytes[2] = b2_present ? raw_data[i + 2] : null_byte;

    base64[base64_length++] = enc_lut[(static_cast<uint8_t>(bytes[0]) & 0xfc) >> 2];
    base64[base64_length++] = enc_lut[(static_cast<uint8_t>(bytes[0]) & 0x03) << 4 | (static_cast<uint8_t>(bytes[1]) & 0xf0) >> 4];
    if (b1_present) {
      base64[base64_length++] = enc_lut[(static_cast<uint8_t>(bytes[1]) & 0x0f) << 2 | (static_cast<uint8_t>(bytes[2]) & 0xc0) >> 6];
    } else if (padded) {
      base64[base64_length++] = '=';
    }
    if (b2_present) {
      base64[base64_length++] = enc_lut[static_cast<uint8_t>(bytes[2]) & 0x3f];
    } else if (padded) {
      base64[base64_length++] = '=';
    }
  }

  return base64_length;
}

std::string to_base64(const std::span<const std::byte> raw_data, bool url /*= false*/, bool padded /*= true*/) {
  std::string buf;
  buf.resize((raw_data.size() / 3 + 1) * 4);
  size_t base64_length = to_base64(buf.data(), raw_data, url, padded);
  gsl_Assert(base64_length <= buf.size());
  buf.resize(base64_length);
  return buf;
}

std::string escapeUnprintableBytes(std::span<const std::byte> data) {
  constexpr const char* hex_digits = "0123456789abcdef";
  std::string result;
  for (auto byte : data) {
    char ch = static_cast<char>(byte);
    if (ch == '\n') {
      result += "\\n";
    } else if (ch == '\t') {
      result += "\\t";
    } else if (ch == '\r') {
      result += "\\r";
    } else if (ch == '\v') {
      result += "\\v";
    } else if (ch == '\f') {
      result += "\\f";
    } else if (std::isprint(static_cast<unsigned char>(byte))) {
      result += ch;
    } else {
      result += "\\x";
      result += hex_digits[(std::to_integer<int>(byte) >> 4) & 0xf];
      result += hex_digits[std::to_integer<int>(byte) & 0xf];
    }
  }
  return result;
}

bool matchesSequence(std::string_view str, const std::vector<std::string>& patterns) {
  size_t pos = 0;

  for (const auto& pattern : patterns) {
    pos = str.find(pattern, pos);
    if (pos == std::string_view::npos) {
      return false;
    }
    pos += pattern.size();
  }

  return true;
}

bool splitToValueAndUnit(std::string_view input, int64_t& value, std::string& unit) {
  const char* begin = input.data();
  const char* end = begin + input.size();
  auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ptr == begin || ec != std::errc()) {
    return false;
  }

  while (ptr != end && *ptr == ' ') {
    // Skip the spaces
    ptr++;
  }
  unit = std::string(ptr, end);
  return true;
}

nonstd::expected<std::optional<char>, ParseError> parseCharacter(const std::string_view input) {
  if (input.empty()) { return std::nullopt; }
  if (input.size() == 1) { return input[0]; }

  if (input.size() == 2 && input.starts_with('\\')) {
    switch (input[1]) {
      case '0': return '\0';  // Null
      case 'a': return '\a';  // Bell
      case 'b': return '\b';  // Backspace
      case 't': return '\t';  // Horizontal Tab
      case 'n': return '\n';  // Line Feed
      case 'v': return '\v';  // Vertical Tab
      case 'f': return '\f';  // Form Feed
      case 'r': return '\r';  // Carriage Return
      case '\\': return '\\';
      default: break;
    }
  }
  return nonstd::make_unexpected(ParseError{});
}

std::string replaceEscapedCharacters(std::string_view input) {
  std::stringstream result;
  for (size_t i = 0; i < input.size(); ++i) {
    char input_char = input[i];
    if (input_char != '\\' || i == input.size() - 1) {
      result << input_char;
      continue;
    }
    char next_char = input[i+1];
    switch (next_char) {
      case '0':
        result << '\0';  // Null
        ++i;
        break;
      case 'a':
        result << '\a';  // Bell
        ++i;
        break;
      case 'b':
        result << '\b';  // Backspace
        ++i;
        break;
      case 't':
        result << '\t';  // Horizontal Tab
        ++i;
        break;
      case 'n':
        result << '\n';  // Line Feed
        ++i;
        break;
      case 'v':
        result << '\v';  // Vertical Tab
        ++i;
        break;
      case 'f':
        result << '\f';  // Form Feed
        ++i;
        break;
      case 'r':
        result << '\r';  // Carriage Return
        ++i;
        break;
      case '\\':
        result << '\\';
        ++i;
        break;
      default:
        result << '\\';
        break;
    }
  }
  return result.str();
}

std::string repeat(std::string_view str, size_t count) {
  std::string result;
  result.reserve(count * str.length());
  for (size_t i = 0; i < count; ++i) {
    result.append(str);
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::utils::string

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

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

bool StringUtils::StringToBool(std::string input, bool &output) {
  std::transform(input.begin(), input.end(), input.begin(), ::tolower);
  std::istringstream(input) >> std::boolalpha >> output;
  return output;
}

std::string StringUtils::trim(std::string s) {
  return trimRight(trimLeft(s));
}

std::vector<std::string> StringUtils::split(const std::string &str, const std::string &delimiter) {
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

bool StringUtils::StringToFloat(std::string input, float &output, FailurePolicy cp /*= RETURN*/) {
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

std::string StringUtils::replaceEnvironmentVariables(std::string& original_string) {
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

std::string& StringUtils::replaceAll(std::string& source_string, const std::string &from_string, const std::string &to_string) {
  std::size_t loc = 0;
  std::size_t lastFound;
  while ((lastFound = source_string.find(from_string, loc)) != std::string::npos) {
    source_string.replace(lastFound, from_string.size(), to_string);
    loc = lastFound + to_string.size();
  }
  return source_string;
}

std::string StringUtils::replaceMap(std::string source_string, const std::map<std::string, std::string> &replace_map) {
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

constexpr uint8_t StringUtils::SKIP;
constexpr uint8_t StringUtils::hex_lut[128];

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

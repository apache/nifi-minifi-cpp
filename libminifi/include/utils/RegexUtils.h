/**
 *
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

#ifndef LIBMINIFI_INCLUDE_IO_REGEXUTILS_H_
#define LIBMINIFI_INCLUDE_IO_REGEXUTILS_H_

#include <vector>
#include <regex>

#if (__cplusplus > 201103L) || defined(_WIN32)
#define NO_MORE_REGFREEE
#endif

#ifndef NO_MORE_REGFREEE
#include <regex.h>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class Regex {
public:
  enum class Mode { ICASE };

  Regex();
  explicit Regex(const std::string &value);
  explicit Regex(const std::string &value,
                const std::vector<Mode> &mode);
  Regex(const Regex &) = delete;
  Regex& operator=(const Regex &) = delete;
  Regex(Regex&& other);
  Regex& operator=(Regex&& other);
  ~Regex();
  bool match(const std::string &pattern);
  const std::vector<std::string>& getResult() const;
  const std::string& getSuffix() const;

 private:
  std::string pat_;
  std::string suffix_;
  std::string regexStr_;
  std::vector<std::string> results_;
  bool valid_;

#ifdef NO_MORE_REGFREEE

  std::regex compiledRegex_;
  std::regex_constants::syntax_option_type regex_mode_;
  std::smatch matches_;

#else

  regex_t compiledRegex_;
  int regex_mode_;
  std::vector<regmatch_t> matches_;

#endif
};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_REGEXUTILS_H_ */

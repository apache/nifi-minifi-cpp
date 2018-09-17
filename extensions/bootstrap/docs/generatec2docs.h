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

#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <regex>
#include <iomanip>
#include "utils/StringUtils.h"

std::string toHex(const std::string& s) {
  std::ostringstream ret;

  for (std::string::size_type i = 0; i < s.length(); ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << (int) s[i];

  return ret.str();
}

int generateC2Docs(const std::string &inputfile, const std::string &output) {
  std::ifstream inf(inputfile);
  std::string input((std::istreambuf_iterator<char>(inf)), std::istreambuf_iterator<char>());
  std::smatch m;
  std::smatch n;
  std::regex e("## ([A-Za-z]+)\\s+### Description\\s");

  std::ofstream outputFile(output);
  outputFile << "/*" \
 "*" \
 "* Licensed to the Apache Software Foundation (ASF) under one or more\n" \
 "* contributor license agreements.  See the NOTICE file distributed with\n" \
 "* this work for additional information regarding copyright ownership.\n" \
 "* The ASF licenses this file to You under the Apache License, Version 2.0\n" \
 "* (the \"License\"); you may not use this file except in compliance with\n" \
 "* the License.  You may obtain a copy of the License at\n" \
     "*\n" \
     "*     http://www.apache.org/licenses/LICENSE-2.0\n" \
     "*\n" \
     "* Unless required by applicable law or agreed to in writing, software\n" \
     "* distributed under the License is distributed on an \"AS IS\" BASIS,\n" \
     "* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n" \
     "* See the License for the specific language governing permissions and\n" \
     "* limitations under the License.\n" \
     "*/\n" \
      "#ifndef AGENT_DOCS_H" << '\n';
  outputFile << "#define AGENT_DOCS_H" << '\n';
  outputFile << "#include <string>\n#include <stdlib.h>\n#include <utils/StringUtils.h>\n";
  outputFile << "namespace org {\n "
             "namespace apache { \n"
             "namespace nifi { \n"
             "namespace minifi { \n"
             "class AgentDocs{ \n"
             " public: \n"
             "     static std::string getDescription(const std::string &feature){ \n"
             "      static std::map<std::string,std::string>  extensions; \n"
             "       if (extensions.empty()){ \n";

  while (std::regex_search(input, m, e)) {
    auto processor = m[1].str();

    input = m.suffix().str();

    auto nextBlock = input.find("###");

    auto description = input.substr(0, nextBlock);

    auto desc = org::apache::nifi::minifi::utils::StringUtils::trim(description);
    outputFile << "     extensions.insert(std::make_pair(\"" << processor << "\",utils::StringUtils::hex_ascii(\"" << toHex(desc) << "\")));\n";
  }

  outputFile << "}\n    return extensions[feature]; \n"
             " \n} \n}; \n} \n} \n} \n} \n"
             "#endif";

  outputFile.close();

  return 0;
}


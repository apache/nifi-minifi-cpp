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
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <queue>
#include <map>
#include <unistd.h>

#include "cxxopts.hpp"
#include "docs/generatec2docs.h"

int main(int argc, char **argv) {
  cxxopts::Options options("Bootstrap", "Build bootstrap");
  options.positional_help("[optional args]").show_positional_help();

  options.add_options()  //NOLINT
  ("h,help", "Shows Help")  //NOLINT
  ("inputc2docs", "Input C2 documentation readme", cxxopts::value<std::string>()) //NOLINT
  ("outputc2docs", "Specifies output directory for generated C2 documentation", cxxopts::value<std::string>());

  try {
    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help( { "", "Group" }) << std::endl;
      exit(0);
    }

    if (result.count("inputc2docs") && result.count("outputc2docs")) {
      generateC2Docs(result["inputc2docs"].as<std::string>(),result["outputc2docs"].as<std::string>());
      std::cout << "Generated docs at " << result["outputc2docs"].as<std::string>() << std::endl;
    }

  }catch (const std::exception &exc)
  {
      // catch anything thrown within try block that derives from std::exception
      std::cerr << exc.what() << std::endl;
  } catch (...) {
    std::cout << options.help( { "", "Group" }) << std::endl;
    exit(0);
  }
  return 0;
}

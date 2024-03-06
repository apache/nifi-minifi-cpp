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

#pragma once

#include <filesystem>
#include <string>

namespace org::apache::nifi::minifi::extensions::python {

struct PythonConfigState {
 public:
  PythonConfigState(PythonConfigState&&) = delete;
  PythonConfigState(const PythonConfigState&) = delete;
  PythonConfigState& operator=(PythonConfigState&&) = delete;
  PythonConfigState& operator=(const PythonConfigState&) = delete;

  bool isPackageInstallationNeeded() const {
    return install_python_packages_automatically && !virtualenv_path.empty();
  }

  static PythonConfigState& getInstance() {
    static PythonConfigState config;
    return config;
  }

  std::filesystem::path virtualenv_path;
  std::string python_binary;
  bool install_python_packages_automatically = false;

 private:
  PythonConfigState() = default;
};

}  // namespace org::apache::nifi::minifi::extensions::python

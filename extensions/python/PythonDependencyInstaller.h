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

#include <memory>
#include <filesystem>

#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::extensions::python {

class PythonDependencyInstaller {
 public:
  explicit PythonDependencyInstaller(const std::shared_ptr<Configure> &configuration);
  void installDependencies(const std::vector<std::filesystem::path>& classpaths) const;

 private:
  std::vector<std::filesystem::path> getRequirementsFilePaths() const;
  void runInstallCommandInVirtualenv(const std::string& install_command) const;
  void createVirtualEnvIfSpecified() const;
  static void evalScript(std::string_view script);
  void addVirtualenvToPath() const;
  bool isPackageInstallationNeeded() const {
    return install_python_packages_automatically_ && !virtualenv_path_.empty();
  }

  std::filesystem::path virtualenv_path_;
  std::filesystem::path python_processor_dir_;
  std::string python_binary_;
  bool install_python_packages_automatically_ = false;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PythonDependencyInstaller>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::python

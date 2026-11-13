// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <filesystem>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/ExtensionInitUtils.h"

namespace org::apache::nifi::minifi::extensions::python {

class PythonDependencyInstaller {
 public:
  explicit PythonDependencyInstaller(const minifi::utils::ConfigReader& config_reader);
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

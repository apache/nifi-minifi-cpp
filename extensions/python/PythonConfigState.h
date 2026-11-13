// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <filesystem>
#include <string>

namespace org::apache::nifi::minifi::extensions::python {

struct PythonConfigState {
  bool isPackageInstallationNeeded() const {
    return install_python_packages_automatically && !virtualenv_path.empty();
  }

  std::filesystem::path virtualenv_path;
  std::filesystem::path python_processor_dir;
  std::string python_binary;
  bool install_python_packages_automatically = false;
};

}  // namespace org::apache::nifi::minifi::extensions::python

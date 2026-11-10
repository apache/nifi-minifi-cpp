// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "Core.h"

namespace org::apache::nifi::minifi::core {

class ObjectFactory {
 public:
  virtual std::unique_ptr<CoreComponent> create(const std::string& /*name*/) = 0;
  virtual gsl::owner<CoreComponent*> createRaw(const std::string& /*name*/) = 0;
  virtual std::unique_ptr<CoreComponent> create(const std::string& /*name*/, const utils::Identifier& /*uuid*/) = 0;
  virtual gsl::owner<CoreComponent*> createRaw(const std::string& /*name*/, const utils::Identifier& /*uuid*/) = 0;
  virtual std::string getModuleName() const = 0;
  virtual std::string getClassName() = 0;

  virtual ~ObjectFactory() = default;
};

}  // namespace org::apache::nifi::minifi::core

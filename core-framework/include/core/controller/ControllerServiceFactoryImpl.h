// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/ClassName.h"
#include "minifi-cpp/core/ControllerServiceMetadata.h"
#include "minifi-cpp/core/controller/ControllerServiceFactory.h"

namespace org::apache::nifi::minifi::core::controller {

template<class T>
class ControllerServiceFactoryImpl : public ControllerServiceFactory {
 public:
  ControllerServiceFactoryImpl()
      : class_name_(core::className<T>()) {
  }

  explicit ControllerServiceFactoryImpl(std::string module_name)
      : module_name_(std::move(module_name)),
        class_name_(core::className<T>()) {
  }

  std::string getModuleName() const override {
    return module_name_;
  }

  std::unique_ptr<ControllerServiceApi> create(ControllerServiceMetadata metadata) override {
    return std::make_unique<T>(metadata);
  }

  std::string getClassName() const override {
    return std::string{class_name_};
  }

 protected:
  std::string module_name_;
  std::string_view class_name_;
};

}  // namespace org::apache::nifi::minifi::core::controller

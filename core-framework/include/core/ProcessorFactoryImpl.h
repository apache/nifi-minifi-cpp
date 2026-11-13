// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "core/ClassName.h"
#include "minifi-cpp/core/ProcessorFactory.h"

namespace org::apache::nifi::minifi::core {

template<class T>
class ProcessorFactoryImpl : public ProcessorFactory {
 public:
  ProcessorFactoryImpl()
      : class_name_(core::className<T>()) {
  }

  explicit ProcessorFactoryImpl(std::string group_name)
      : group_name_(std::move(group_name)),
        class_name_(core::className<T>()) {
  }

  std::string getGroupName() const override {
    return group_name_;
  }

  std::unique_ptr<ProcessorApi> create(ProcessorMetadata metadata) override {
    return std::make_unique<T>(metadata);
  }

  std::string getClassName() const override {
    return std::string{class_name_};
  }

 protected:
  std::string group_name_;
  std::string_view class_name_;
};

}  // namespace org::apache::nifi::minifi::core

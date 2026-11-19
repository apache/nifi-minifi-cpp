// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "core/ClassName.h"
#include "minifi-cpp/core/reporting/ReportingTaskFactory.h"

namespace org::apache::nifi::minifi::core::reporting {

template<class T>
class ReportingTaskFactoryImpl : public ReportingTaskFactory {
 public:
  ReportingTaskFactoryImpl()
      : class_name_(core::className<T>()) {
  }

  explicit ReportingTaskFactoryImpl(std::string group_name)
      : group_name_(std::move(group_name)),
        class_name_(core::className<T>()) {
  }

  std::string getModuleName() const override {
    return group_name_;
  }

  std::unique_ptr<ReportingTaskApi> create(ReportingTaskMetadata metadata) override {
    return std::make_unique<T>(std::move(metadata));
  }

  std::string getClassName() const override {
    return std::string{class_name_};
  }

 protected:
  std::string group_name_;
  std::string_view class_name_;
};

}  // namespace org::apache::nifi::minifi::core::reporting

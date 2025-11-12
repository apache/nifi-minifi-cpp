/**
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

#include <string>
#include <memory>
#include <utility>
#include "core/ClassName.h"
#include "minifi-cpp/core/controller/ControllerServiceFactory.h"

namespace org::apache::nifi::minifi::core::controller {

template<class T>
class ControllerServiceFactoryImpl : public ControllerServiceFactory {
 public:
  ControllerServiceFactoryImpl()
      : class_name_(core::className<T>()) {
  }

  explicit ControllerServiceFactoryImpl(std::string group_name)
      : group_name_(std::move(group_name)),
        class_name_(core::className<T>()) {
  }

  std::string getGroupName() const override {
    return group_name_;
  }

  std::unique_ptr<ControllerServiceApi> create(ControllerServiceMetadata metadata) override {
    return std::make_unique<T>(metadata);
  }

  std::string getClassName() const override {
    return std::string{class_name_};
  }

 protected:
  std::string group_name_;
  std::string_view class_name_;
};

}  // namespace org::apache::nifi::minifi::core::controller

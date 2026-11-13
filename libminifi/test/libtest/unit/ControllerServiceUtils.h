// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string_view>

#include "core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::test::utils {

template<typename T>
std::unique_ptr<core::controller::ControllerService> make_controller_service(std::string_view name, std::optional<minifi::utils::Identifier> uuid = std::nullopt) {
  if (!uuid) {
    uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
  }
  auto controller_service_impl = std::make_unique<T>(core::ControllerServiceMetadata{
      .uuid = uuid.value(),
      .name = std::string{name},
      .logger = minifi::core::logging::LoggerFactory<T>::getLogger(uuid.value())
  });
  return std::make_unique<core::controller::ControllerService>(name, uuid.value(), std::move(controller_service_impl));
}

}  // namespace org::apache::nifi::minifi::test::utils

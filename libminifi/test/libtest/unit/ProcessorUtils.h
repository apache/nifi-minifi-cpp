// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "core/Processor.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::test::utils {

template<typename T>
std::unique_ptr<core::Processor> make_processor(std::string_view name, std::optional<minifi::utils::Identifier> uuid = std::nullopt) {
  if (!uuid) {
    uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
  }
  auto processor_impl = std::make_unique<T>(core::ProcessorMetadata{
      .uuid = uuid.value(),
      .name = std::string{name},
      .logger = minifi::core::logging::LoggerFactory<T>::getLogger(uuid.value())
  });
  return std::make_unique<core::Processor>(minifi::utils::string::partAfterLastOccurrenceOf(core::className<T>(), ':'),  name, uuid.value(), std::move(processor_impl));
}

template<typename T, typename ...Args>
std::unique_ptr<core::Processor> make_custom_processor(Args&&... args) {
  auto processor_impl = std::make_unique<T>(std::forward<Args>(args)...);
  auto name = processor_impl->getName();
  auto uuid = processor_impl->getUUID();
  return std::make_unique<core::Processor>(minifi::utils::string::partAfterLastOccurrenceOf(core::className<T>(), ':'), name, uuid, std::move(processor_impl));
}

}  // namespace org::apache::nifi::minifi::test::utils

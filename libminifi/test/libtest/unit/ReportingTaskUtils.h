// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "core/reporting/ReportingTask.h"

namespace org::apache::nifi::minifi::test::utils {

template<typename T>
std::unique_ptr<core::Processor> make_reporting_task(std::string_view name, std::optional<minifi::utils::Identifier> uuid = std::nullopt) {
  if (!uuid) {
    uuid = minifi::utils::IdGenerator::getIdGenerator()->generate();
  }
  auto reporting_task_impl = std::make_unique<T>(core::reporting::ReportingTaskMetadata{
      .uuid = uuid.value(),
      .name = std::string{name},
      .logger = minifi::core::logging::LoggerFactory<T>::getLogger(uuid.value())
  });
  return std::make_unique<core::Processor>(minifi::utils::string::partAfterLastOccurrenceOf(core::className<T>(), ':'), name, uuid.value(),
      std::make_unique<minifi::core::reporting::ReportingTask>(std::move(reporting_task_impl)));
}

}  // namespace org::apache::nifi::minifi::test::utils

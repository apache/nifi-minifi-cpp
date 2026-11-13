// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>

#include "minifi-cpp/core/Property.h"
#include "core/TypedValues.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core {

const  std::type_index DataSizeValue::type_id = typeid(uint64_t);
const  std::type_index DataTransferSpeedValue::type_id = typeid(uint64_t);
const  std::type_index TimePeriodValue::type_id = typeid(uint64_t);

std::shared_ptr<logging::Logger>& DataSizeValue::getLogger() {
  static std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<DataSizeValue>::getLogger();
  return logger;
}

}  // namespace org::apache::nifi::minifi::core

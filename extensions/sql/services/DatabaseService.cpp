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

#include "core/logging/LoggerFactory.h"
#include "core/controller/ControllerService.h"
#include <string>
#include <memory>
#include <set>
#include "DatabaseService.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::sql::controllers {

void DatabaseService::initialize() {
  std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);

  if (initialized_) {
    return;
  }

  ControllerServiceImpl::initialize();

  initializeProperties();

  initialized_ = true;
}

void DatabaseService::onEnable() {
  getProperty(ConnectionString, connection_string_);
}

void DatabaseService::initializeProperties() {
  setSupportedProperties(Properties);
}

}  // namespace org::apache::nifi::minifi::sql::controllers

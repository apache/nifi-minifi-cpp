/**
 *
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

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"
#include <string>
#include <memory>
#include <set>
#include "core/Property.h"
#include "DatabaseService.h"
#include "DatabaseService.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {
namespace controllers {

static core::Property RemoteServer;
static core::Property Port;
static core::Property MaxQueueSize;

core::Property DatabaseService::ConnectionString(core::PropertyBuilder::createProperty("Connection String")->withDescription("Database Connection String")->isRequired(true)->build());

void DatabaseService::initialize() {
  if (initialized_)
    return;

  std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
  
  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

void DatabaseService::onEnable() {
  getProperty(ConnectionString.getName(), connection_string_);
}

void DatabaseService::initializeProperties() {
  setSupportedProperties( { ConnectionString });
}

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

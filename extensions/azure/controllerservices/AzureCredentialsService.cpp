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

#include "AzureCredentialsService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace azure {
namespace controllers {

core::Property AzureCredentialsService::ConnectionString(
    core::PropertyBuilder::createProperty("Connection String")->withDescription("Connection string for authenticating with Azure storage service.")->isRequired(true)->supportsExpressionLanguage(true)->build());

void AzureCredentialsService::initialize() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(ConnectionString);
  setSupportedProperties(supportedProperties);
}

void AzureCredentialsService::onEnable() {
  getProperty(ConnectionString.getName(), connection_string_);
}

}  // namespace controllers
}  // namespace azure
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

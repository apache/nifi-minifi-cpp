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


#include "controllers/AttributeProviderService.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::controllers {

AttributeProviderService* AttributeProviderService::getFromProperty(const core::ProcessContext& context, const core::Property& attributeProviderService) {
  const auto attribute_provider_service_name = context.getProperty(attributeProviderService);
  if (!attribute_provider_service_name || attribute_provider_service_name->empty()) {
    return nullptr;
  }

  std::shared_ptr <core::controller::ControllerService> controller_service = context.getControllerService(*attribute_provider_service_name);
  if (!controller_service) {
    throw Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::StringUtils::join_pack("Controller service '", *attribute_provider_service_name, "' not found")};
  }

  // we drop ownership of the service here -- in the long term, getControllerService() should return a non-owning pointer or optional reference
  auto *ret = dynamic_cast<AttributeProviderService *>(controller_service.get());
  if (!ret) {
    throw Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::StringUtils::join_pack("Controller service '", *attribute_provider_service_name, "' is not an AttributeProviderService")};
  }

  return ret;
}

}  // namespace org::apache::nifi::minifi::controllers

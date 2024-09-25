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

#include "core/controller/ControllerServiceProvider.h"

#include <memory>
#include <string>

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core::controller {

std::shared_ptr<ControllerService> ControllerServiceProviderImpl::getControllerService(const std::string &identifier) const {
  auto service = controller_map_->get(identifier);
  if (service != nullptr) {
    return service->getControllerServiceImplementation();
  } else {
    return nullptr;
  }
}

void ControllerServiceProviderImpl::putControllerServiceNode(const std::string& identifier, const std::shared_ptr<ControllerServiceNode>& controller_service_node) {
  gsl_Expects(controller_map_);
  controller_map_->put(identifier, controller_service_node);
}

}  // namespace org::apache::nifi::minifi::core::controller

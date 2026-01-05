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

#include "ControllerServiceInterface.h"
#include "ControllerServiceContext.h"
#include "ControllerServiceDescriptor.h"
#include "minifi-cpp/properties/Configure.h"

namespace org::apache::nifi::minifi::core::controller {

class ControllerServiceApi {
  public:
    virtual ~ControllerServiceApi() = default;

    virtual void initialize(ControllerServiceDescriptor& descriptor) = 0;
    virtual void onEnable(ControllerServiceContext& context, const std::shared_ptr<Configure>& configuration, const std::vector<std::shared_ptr<ControllerServiceInterface>>& linked_services) = 0;
    virtual void notifyStop() = 0;
    virtual ControllerServiceInterface* getControllerServiceInterface() = 0;
};

}  // namespace org::apache::nifi::minifi::core::controller

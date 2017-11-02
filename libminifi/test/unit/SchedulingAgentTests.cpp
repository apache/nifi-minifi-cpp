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

#include <memory>
#include <string>
#include <vector>
#include "io/CRCStream.h"
#include "io/DataStream.h"
#include "../TestBase.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "SchedulingAgent.h"
#include "TimerDrivenSchedulingAgent.h"


TEST_CASE("TestTDAgent", "[test1]") {
  std::shared_ptr<core::Processor> procA = std::make_shared<minifi::processors::GetFile>("getFile");
  std::shared_ptr<core::Processor> procB = std::make_shared<minifi::processors::LogAttribute>("logAttribute");
  // agent.run()
}

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
#include "cxx/CallbackProcessor.h"
#include "core/cxxstructs.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship CallbackProcessor::Success("success", "All files are routed to success");
core::Relationship CallbackProcessor::Failure("failure", "Failed files (based on callback logic) are transferred to failure");

void CallbackProcessor::initialize() {
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void CallbackProcessor::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  if (onschedule_callback_ != nullptr) {
    onschedule_callback_(context);
  }
}

void CallbackProcessor::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
 if (ontrigger_callback_ != nullptr) {
   ontrigger_callback_(session, context);
 }
}

REGISTER_RESOURCE(CallbackProcessor, "");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

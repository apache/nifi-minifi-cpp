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
#ifndef LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_
#define LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_

#include "core/Connectable.h"
#include "c2/C2Payload.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Purpose: Defines basic triggering mechanism for command and control interfaces
 *
 * Design: Extends Connectable so that we can instantiate with the class name
 */
class C2Trigger : public core::Connectable{
 public:

  C2Trigger(std::string name, utils::Identifier uuid)
        : core::Connectable(name, uuid){

  }
  virtual ~C2Trigger() {
  }


  /**
   * initializes trigger with minifi configuration.
   */
  virtual void initialize(const std::shared_ptr<minifi::Configure> &configuration) = 0;
  /**
   * returns true if triggered
   */
  virtual bool triggered() = 0;

  /**
   * Returns a payload implementing a C2 action
   */
  virtual C2Payload getAction() = 0;
};

} /* namesapce c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_C2_C2TRIGGER_H_ */

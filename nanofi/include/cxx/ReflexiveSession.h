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
#ifndef __REFLEXIVE_SESSION_H__
#define __REFLEXIVE_SESSION_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>

#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ReflexiveSession Class
class ReflexiveSession : public ProcessSession{
 public:
  // Constructor
  /*!
   * Create a new process session
   */
  ReflexiveSession(std::shared_ptr<ProcessContext> processContext = nullptr)
      : ProcessSession(processContext){
  }

// Destructor
  virtual ~ReflexiveSession() {
  }

   virtual std::shared_ptr<core::FlowFile> get(){
     auto prevff = ff;
     ff = nullptr;
     return prevff;
   }

   virtual void add(const std::shared_ptr<core::FlowFile> &flow){
     ff = flow;
   }
   virtual void transfer(const std::shared_ptr<core::FlowFile> &flow, Relationship relationship){
     // no op
   }
 protected:
  //
  // Get the FlowFile from the highest priority queue
  std::shared_ptr<core::FlowFile> ff;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif

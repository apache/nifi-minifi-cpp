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
#pragma once

#include "utils/BackTrace.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Purpose: Defines a resource eligible for tracing
 *
 * Design: Pure virtual class -- when used, the developer must
 * implement getTraces. This is intentional because every use case
 * of TraceableResource will be specific to the type of threaded implementation.
 *
 */
class TraceableResource {
 public:
  virtual ~TraceableResource() = default;
  /**
   * Since SerializableComponents represent a runnable object, we should return traces
   */
  virtual BackTrace getTraces() = 0;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

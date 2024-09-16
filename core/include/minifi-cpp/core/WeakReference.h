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

#ifndef LIBMINIFI_INCLUDE_CORE_WEAKREFERENCE_H_
#define LIBMINIFI_INCLUDE_CORE_WEAKREFERENCE_H_

#include <memory>

#include  <type_traits>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/*
 * An homage to weak references in java, this acts as a class
 * which can be used to remove referenced classes when needed.
 */
class WeakReference {
 public:
  virtual ~WeakReference() = default;
  virtual void remove() = 0;
};

/**
 * Reference container is a vector of weak references that enables
 * controllers to remove referenced objects as needed.
 *
 * There is no need to use weak ptrs here, as we do actually want
 * the WeakReferences to be referenced counts. The "weak" aspect
 * originates from and is defined by the corresponding object.
 */
class ReferenceContainer {
 public:
  virtual ~ReferenceContainer() = default;
  virtual void addReference(std::shared_ptr<WeakReference> ref) = 0;
  virtual size_t getReferenceCount() = 0;
  virtual void removeReferences() = 0;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_WEAKREFERENCE_H_

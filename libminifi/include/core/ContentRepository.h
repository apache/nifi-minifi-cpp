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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_

#include "properties/Configure.h"
#include "ResourceClaim.h"
#include "io/DataStream.h"
#include "io/BaseStream.h"
#include "StreamManager.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepository : public StreamManager<minifi::ResourceClaim> {
 public:
  virtual ~ContentRepository() {

  }

  /**
   * initialize this content repository using the provided configuration.
   */
  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;

  /**
   * Stops this repository.
   */
  virtual void stop() = 0;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_ */

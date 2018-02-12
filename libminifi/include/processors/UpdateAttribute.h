/**
 * @file UpdateAttribute.h
 * UpdateAttribute class declaration
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
#ifndef __UPDATE_ATTRIBUTE_H__
#define __UPDATE_ATTRIBUTE_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class UpdateAttribute : public core::Processor {
 public:

  UpdateAttribute(std::string name, uuid_t uuid = NULL)
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<UpdateAttribute>::getLogger()) {
  }

  /**
   * Relationships
   */

  static core::Relationship Success;
  static core::Relationship Failure;

  /**
   * NiFi API implementation
   */

  virtual bool supportsDynamicProperties() {
    return true;
  };

  virtual void onSchedule(core::ProcessContext *context,
                          core::ProcessSessionFactory *sessionFactory);
  virtual void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session);
  virtual void initialize(void);

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::vector<std::string> attributes_;
};

REGISTER_RESOURCE(UpdateAttribute);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* __UPDATE_ATTRIBUTE_H__ */

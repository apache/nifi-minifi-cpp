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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_FileSystemRepository_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_FileSystemRepository_H_

#include "core/Core.h"
#include "../ContentRepository.h"
#include "properties/Configure.h"
#include "core/logging/LoggerConfiguration.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * FileSystemRepository is a content repository that stores data onto the local file system.
 */
class FileSystemRepository : public core::ContentRepository, public core::CoreComponent {
 public:
  FileSystemRepository(std::string name = getClassName<FileSystemRepository>())
      : core::CoreComponent(name),
        logger_(logging::LoggerFactory<FileSystemRepository>::getLogger()) {

  }
  virtual ~FileSystemRepository() {

  }

  virtual bool initialize(const std::shared_ptr<minifi::Configure> &configuration);

  virtual void stop();

  bool exists(const std::shared_ptr<minifi::ResourceClaim> &streamId);

  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<minifi::ResourceClaim> &claim);

  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<minifi::ResourceClaim> &claim);

  virtual bool close(const std::shared_ptr<minifi::ResourceClaim> &claim) {
    return remove(claim);
  }

  virtual bool remove(const std::shared_ptr<minifi::ResourceClaim> &claim);

 private:

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_FileSystemRepository_H_ */

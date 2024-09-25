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
#include "core/RepositoryFactory.h"
#include <memory>
#include <string>
#include <algorithm>
#include "core/ContentRepository.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/Repository.h"
#include "core/ClassLoader.h"
#include "core/repository/FileSystemRepository.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "core/repository/VolatileProvenanceRepository.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

std::unique_ptr<core::ContentRepository> createContentRepository(const std::string& configuration_class_name, bool fail_safe, const std::string& repo_name) {
  std::string class_name_lc = configuration_class_name;
  std::transform(class_name_lc.begin(), class_name_lc.end(), class_name_lc.begin(), ::tolower);
  try {
    auto return_obj = core::ClassLoader::getDefaultClassLoader().instantiate<core::ContentRepository>(class_name_lc,
                                                                                                      class_name_lc);
    if (return_obj) {
      return_obj->setName(repo_name);
      return return_obj;
    }
    if (class_name_lc == "volatilecontentrepository") {
      return std::make_unique<core::repository::VolatileContentRepository>(repo_name);
    } else if (class_name_lc == "filesystemrepository") {
      return std::make_unique<core::repository::FileSystemRepository>(repo_name);
    }
    if (fail_safe) {
      return std::make_unique<core::repository::VolatileContentRepository>("fail_safe");
    } else {
      throw std::runtime_error("Support for the provided configuration class could not be found");
    }
  } catch (const std::runtime_error&) {
    if (fail_safe) {
      return std::make_unique<core::repository::VolatileContentRepository>("fail_safe");
    }
  }

  throw std::runtime_error("Support for the provided configuration class could not be found");
}

class NoOpThreadedRepository : public core::ThreadedRepositoryImpl {
 public:
  explicit NoOpThreadedRepository(std::string_view repo_name)
    : ThreadedRepositoryImpl(repo_name) {
  }

  NoOpThreadedRepository(NoOpThreadedRepository&&) = delete;
  NoOpThreadedRepository(const NoOpThreadedRepository&) = delete;
  NoOpThreadedRepository& operator=(NoOpThreadedRepository&&) = delete;
  NoOpThreadedRepository& operator=(const NoOpThreadedRepository&) = delete;

  ~NoOpThreadedRepository() override {
    stop();
  }

  uint64_t getRepositorySize() const override {
    return 0;
  }

  uint64_t getRepositoryEntryCount() const override {
    return 0;
  }

 private:
  void run() override {
  }

  std::thread& getThread() override {
    return thread_;
  }

  std::thread thread_;
};

std::unique_ptr<core::Repository> createRepository(const std::string& configuration_class_name, const std::string& repo_name) {
  std::string class_name_lc = configuration_class_name;
  std::transform(class_name_lc.begin(), class_name_lc.end(), class_name_lc.begin(), ::tolower);
  auto return_obj = core::ClassLoader::getDefaultClassLoader().instantiate<core::ThreadedRepository>(class_name_lc,
                                                                                                     class_name_lc);
  if (return_obj) {
    return_obj->setName(repo_name);
    return return_obj;
  }
  // if the desired repos don't exist, we can try doing string matches and rely on volatile repositories
  if (class_name_lc == "flowfilerepository" || class_name_lc == "volatileflowfilerepository") {
    return instantiate<repository::VolatileFlowFileRepository>(repo_name);
  } else if (class_name_lc == "provenancerepository" || class_name_lc == "volatileprovenancerepository") {
    return instantiate<repository::VolatileProvenanceRepository>(repo_name);
  } else if (class_name_lc == "nooprepository") {
    return std::make_unique<core::NoOpThreadedRepository>(repo_name);
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::core

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
#include "core/Repository.h"
#ifdef LEVELDB_SUPPORT
#include "core/repository/FlowFileRepository.h"
#include "provenance/ProvenanceRepository.h"
#endif

#include "core/repository/VolatileRepository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
#ifndef LEVELDB_SUPPORT
namespace provenance {
class ProvenanceRepository;
}
#endif
namespace core {

#ifndef LEVELDB_SUPPORT
class FlowFileRepository;
#endif

std::shared_ptr<core::Repository> createRepository(
    const std::string configuration_class_name, bool fail_safe, const std::string repo_name) {
  std::shared_ptr<core::Repository> return_obj = nullptr;
  std::string class_name_lc = configuration_class_name;
  std::transform(class_name_lc.begin(), class_name_lc.end(),
                 class_name_lc.begin(), ::tolower);
  try {
    std::shared_ptr<core::Repository> return_obj = nullptr;
    if (class_name_lc == "flowfilerepository") {
      std::cout << "creating flow" << std::endl;
      return_obj = instantiate<core::repository::FlowFileRepository>(repo_name);
    } else if (class_name_lc == "provenancerepository") {
      return_obj = instantiate<provenance::ProvenanceRepository>(repo_name);
    } else if (class_name_lc == "volatilerepository") {
      return_obj = instantiate<repository::VolatileRepository>(repo_name);
    } else if (class_name_lc == "nooprepository") {
      std::cout << "creating noop" << std::endl;
      return_obj = instantiate<core::Repository>(repo_name);
    }

    if (return_obj) {
      return return_obj;
    }
    if (fail_safe) {
      return std::make_shared<core::Repository>("fail_safe", "fail_safe", 1, 1,
                                                1);
    } else {
      throw std::runtime_error(
          "Support for the provided configuration class could not be found");
    }
  } catch (const std::runtime_error &r) {
    if (fail_safe) {
      return std::make_shared<core::Repository>("fail_safe", "fail_safe", 1, 1,
                                                1);
    }
  }

  throw std::runtime_error(
      "Support for the provided configuration class could not be found");
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

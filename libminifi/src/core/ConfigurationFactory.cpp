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
#include <type_traits>
#include <utility>
#include <string>
#include <memory>
#include <algorithm>
#include <set>
#include "core/Core.h"
#include "core/ConfigurationFactory.h"
#include "core/FlowConfiguration.h"
#include "io/StreamFactory.h"

#include "core/yaml/YamlConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::unique_ptr<core::FlowConfiguration> createFlowConfiguration(std::shared_ptr<core::Repository> repo, std::shared_ptr<core::Repository> flow_file_repo,
                                                                 std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configure,
                                                                 std::shared_ptr<io::StreamFactory> stream_factory, const std::string configuration_class_name, const std::string path,
                                                                 bool fail_safe) {
  std::string class_name_lc = configuration_class_name;
  std::transform(class_name_lc.begin(), class_name_lc.end(), class_name_lc.begin(), ::tolower);
  try {
    if (class_name_lc == "flowconfiguration") {
      // load the base configuration.

      return std::unique_ptr<core::FlowConfiguration>(new core::FlowConfiguration(repo, flow_file_repo, content_repo, stream_factory, configure, path));

    } else if (class_name_lc == "yamlconfiguration") {
      // only load if the class is defined.
      return std::unique_ptr<core::FlowConfiguration>(instantiate<core::YamlConfiguration>(repo, flow_file_repo, content_repo, stream_factory, configure, path));

    } else {
      if (fail_safe) {
        return std::unique_ptr<core::FlowConfiguration>(new core::FlowConfiguration(repo, flow_file_repo, content_repo, stream_factory, configure, path));
      } else {
        throw std::runtime_error("Support for the provided configuration class could not be found");
      }
    }
  } catch (const std::runtime_error &r) {
    if (fail_safe) {
      return std::unique_ptr<core::FlowConfiguration>(new core::FlowConfiguration(repo, flow_file_repo, content_repo, stream_factory, configure, path));
    }
  }

  throw std::runtime_error("Support for the provided configuration class could not be found");
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


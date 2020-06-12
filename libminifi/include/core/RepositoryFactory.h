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

#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORYFACTORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORYFACTORY_H_

#include <memory>
#include <string>

#include "core/ContentRepository.h"
#include "core/Repository.h"
#include "Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

namespace core {

/**
 * Create a repository represented by the configuration class name
 * @param configuration_class_name configuration class name
 * @param fail_safe determines whether or not to make the default class if configuration_class_name is invalid
 * @param repo_name name of the repository
 */
std::shared_ptr<core::Repository> createRepository(const std::string configuration_class_name, bool fail_safe = false, const std::string repo_name = "");

/**
 * Create a context repository
 * @param configuration_class_name configuration class name
 * @param fail_safe determines whether or not to make the default class if configuration_class_name is invalid
 * @param repo_name name of the repository
 */
std::shared_ptr<core::ContentRepository> createContentRepository(const std::string configuration_class_name, bool fail_safe = false, const std::string repo_name = "");

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_REPOSITORYFACTORY_H_

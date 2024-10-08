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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "agent_docs.h"

namespace org::apache::nifi::minifi {

struct BundleDetails {
  std::string artifact;
  std::string group;
  std::string version;
};

class ExternalBuildDescription {
 private:
  static std::vector<BundleDetails> &getExternal();

  static std::map<std::string, Components> &getExternalMappings();

 public:
  static void addExternalComponent(const BundleDetails& details, const ClassDescription& description);

  static Components getClassDescriptions(const std::string &group);

  static std::vector<BundleDetails> getExternalGroups();
};

}  // namespace org::apache::nifi::minifi

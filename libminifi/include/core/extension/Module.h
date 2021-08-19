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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/logging/Logger.h"
#include "utils/gsl.h"
#include "properties/Configure.h"
#include "Extension.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

/**
 * Represents an initializable component of the agent.
 */
class Module {
  friend class ExtensionManager;

 protected:
  explicit Module(std::string name);

 public:
  virtual ~Module();

  std::string getName() const;

  void registerExtension(Extension& extension);
  bool unregisterExtension(Extension& extension);

 private:
  bool initialize(const std::shared_ptr<Configure>& config);

 protected:
  std::string name_;

  std::mutex mtx_;
  bool initialized_{false};
  std::vector<Extension*> extensions_;

  static std::shared_ptr<logging::Logger> logger_;
};

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

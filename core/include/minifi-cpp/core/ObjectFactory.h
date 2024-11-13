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

#include <string>
#include <memory>
#include <utility>
#include "Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
class ObjectFactory {
 public:
  virtual std::unique_ptr<CoreComponent> create(const std::string& /*name*/) = 0;
  virtual CoreComponent *createRaw(const std::string& /*name*/) = 0;
  virtual std::unique_ptr<CoreComponent> create(const std::string& /*name*/, const utils::Identifier& /*uuid*/) = 0;
  virtual CoreComponent* createRaw(const std::string& /*name*/, const utils::Identifier& /*uuid*/) = 0;
  virtual std::string getGroupName() const = 0;
  virtual std::string getClassName() = 0;

  virtual ~ObjectFactory() = default;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

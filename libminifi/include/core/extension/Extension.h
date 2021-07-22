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
#include <vector>
#include <string>

#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

class Extension;

using ExtensionConfig = std::shared_ptr<org::apache::nifi::minifi::Configure>;
using ExtensionInit = bool(*)(Extension*, const ExtensionConfig&);

class ExtensionInitializer;

class Extension {
  friend class ExtensionInitializer;
 public:
  explicit Extension(std::string name, ExtensionInit init);
  virtual ~Extension();

  bool initialize(const ExtensionConfig& config) {
    return init_(this, config);
  }

  const std::string& getName() const {
    return name_;
  }

 protected:
  virtual bool doInitialize(const ExtensionConfig& /*config*/) {
    return true;
  }

  virtual void doDeinitialize() {}

 private:
  std::string name_;
  ExtensionInit init_;
};

class ExtensionInitializer {
 public:
  explicit ExtensionInitializer(Extension* extension, const ExtensionConfig& config);
  ~ExtensionInitializer();

 private:
  Extension* extension_;
};

#define REGISTER_EXTENSION(clazz) \
  static clazz extension_registrar(#clazz, [](org::apache::nifi::minifi::core::extension::Extension* extension, const org::apache::nifi::minifi::core::extension::ExtensionConfig& config) -> bool { \
    try {                             \
      static org::apache::nifi::minifi::core::extension::ExtensionInitializer initializer(extension, config);                                                  \
      return true; \
    } catch (...) {                   \
      return false;                                  \
    }                                 \
  })

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

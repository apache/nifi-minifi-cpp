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
  ObjectFactory(const std::string &group) // NOLINT
      : group_(group) {
  }

  ObjectFactory() = default;

  /**
   * Virtual destructor.
   */
  virtual ~ObjectFactory() = default;

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string& /*name*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent *createRaw(const std::string& /*name*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  virtual std::string getGroupName() const {
    return group_;
  }

  virtual std::string getClassName() = 0;

 private:
  std::string group_;
};
/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
template<class T>
class DefautObjectFactory : public ObjectFactory {
 public:
  DefautObjectFactory() {
    className = core::getClassName<T>();
  }

  DefautObjectFactory(const std::string &group_name) // NOLINT
      : ObjectFactory(group_name) {
    className = core::getClassName<T>();
  }
  /**
   * Virtual destructor.
   */
  virtual ~DefautObjectFactory() = default;

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name) {
    std::shared_ptr<T> ptr = std::make_shared<T>(name);
    return std::static_pointer_cast<CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name, const utils::Identifier &uuid) {
    std::shared_ptr<T> ptr = std::make_shared<T>(name, uuid);
    return std::static_pointer_cast<CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string &name) {
    T *ptr = new T(name);
    return dynamic_cast<CoreComponent*>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string &name, const utils::Identifier &uuid) {
    T *ptr = new T(name, uuid);
    return dynamic_cast<CoreComponent*>(ptr);
  }

  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::string getClassName() {
    return className;
  }

 protected:
  std::string className;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

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

#ifndef LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_
#define LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_

#include <mutex>
#include <vector>
#include <map>
#include "utils/StringUtils.h"
#include <dlfcn.h>
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "io/DataStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define RESOURCE_FAILURE -1

#define RESOURCE_SUCCESS 1

/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
class ObjectFactory {

 public:
  /**
   * Virtual destructor.
   */
  virtual ~ObjectFactory() {

  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent *createRaw(const std::string &name) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name, uuid_t uuid) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string &name, uuid_t uuid) {
    return nullptr;
  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() = 0;

  virtual std::string getClassName() = 0;
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() = 0;

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) = 0;

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
  /**
   * Virtual destructor.
   */
  virtual ~DefautObjectFactory() {

  }

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
  virtual std::shared_ptr<CoreComponent> create(const std::string &name, uuid_t uuid) {
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
  virtual CoreComponent* createRaw(const std::string &name, uuid_t uuid) {
    T *ptr = new T(name, uuid);
    return dynamic_cast<CoreComponent*>(ptr);
  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return className;
  }

  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::string getClassName() {
    return className;
  }

  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> container;
    container.push_back(className);
    return container;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) {
    return nullptr;
  }

 protected:
  std::string className;

};

/**
 * Function that is used to create the
 * processor factory from the shared object.
 */
typedef ObjectFactory* createFactory();

/**
 * Processor class loader that accepts
 * a variety of mechanisms to load in shared
 * objects.
 */
class ClassLoader {

 public:

  static ClassLoader &getDefaultClassLoader();

  /**
   * Constructor.
   */
  ClassLoader();

  ~ClassLoader() {
    loaded_factories_.clear();
    for (auto ptr : dl_handles_) {
      dlclose(ptr);
    }
  }

  /**
   * Register the file system resource.
   * This will attempt to load objects within this resource.
   * @return return code: RESOURCE_FAILURE or RESOURCE_SUCCESS
   */
  uint16_t registerResource(const std::string &resource, const std::string &resourceName);

  /**
   * Register a class with the give ProcessorFactory
   */
  void registerClass(const std::string &name, std::unique_ptr<ObjectFactory> factory) {
    if (loaded_factories_.find(name) != loaded_factories_.end()) {
      return;
    }

    std::lock_guard<std::mutex> lock(internal_mutex_);

    loaded_factories_.insert(std::make_pair(name, std::move(factory)));
  }

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, const std::string &name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, uuid_t uuid);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string &class_name, const std::string &name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string &class_name, uuid_t uuid);

 protected:

  std::map<std::string, std::unique_ptr<ObjectFactory>> loaded_factories_;

  std::mutex internal_mutex_;

  std::vector<void *> dl_handles_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(name);
    return std::dynamic_pointer_cast<T>(obj);
  } else {
    return nullptr;
  }
}

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, uuid_t uuid) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(class_name, uuid);
    return std::dynamic_pointer_cast<T>(obj);
  } else {
    return nullptr;
  }
}

template<class T>
T *ClassLoader::instantiateRaw(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->createRaw(name);
    return dynamic_cast<T*>(obj);
  } else {
    return nullptr;
  }
}

template<class T>
T *ClassLoader::instantiateRaw(const std::string &class_name, uuid_t uuid) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->createRaw(class_name, uuid);
    return dynamic_cast<T*>(obj);
  } else {
    return nullptr;
  }
}

}/* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_ */

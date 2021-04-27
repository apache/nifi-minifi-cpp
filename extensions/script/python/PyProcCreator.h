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
#ifndef EXTENSIONS_PYPROCESS_PYPROCCREATOR_H_
#define EXTENSIONS_PYPROCESS_PYPROCCREATOR_H_

#include <map>
#include "core/ClassLoader.h"
#include "ExecutePythonProcessor.h"
#include "utils/StringUtils.h"

#pragma GCC visibility push(hidden)

class PythonObjectFactory : public core::DefautObjectFactory<minifi::python::processors::ExecutePythonProcessor> {
 public:

  explicit PythonObjectFactory(const std::string &file, const std::string &name)
      : file_(file),
        name_(name) {
  }

  /**
   * Create a shared pointer to a new processor.
   */
  std::shared_ptr<core::CoreComponent> create(const std::string &name) override {
    auto ptr = std::static_pointer_cast<minifi::python::processors::ExecutePythonProcessor>(DefautObjectFactory::create(name));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return std::static_pointer_cast<core::CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  std::shared_ptr<core::CoreComponent> create(const std::string &name, const utils::Identifier &uuid) override {
    auto ptr = std::static_pointer_cast<minifi::python::processors::ExecutePythonProcessor>(DefautObjectFactory::create(name, uuid));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return std::static_pointer_cast<core::CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  core::CoreComponent* createRaw(const std::string &name) override {
    auto ptr = dynamic_cast<minifi::python::processors::ExecutePythonProcessor*>(DefautObjectFactory::createRaw(name));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return dynamic_cast<core::CoreComponent*>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  core::CoreComponent* createRaw(const std::string &name, const utils::Identifier &uuid) override {
    auto ptr = dynamic_cast<minifi::python::processors::ExecutePythonProcessor*>(DefautObjectFactory::createRaw(name, uuid));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return dynamic_cast<core::CoreComponent*>(ptr);
  }
 private:
  std::string file_;
  std::string name_;
};

class PyProcCreator {
 public:

  void addClassName(const std::string &name, std::string file) {
    file_mapping_[name] = file;
  }

  static PyProcCreator *getPythonCreator();

  std::vector<std::string> getClassNames() {
    std::vector<std::string> class_names;
    for (const auto &kv : file_mapping_) {
      class_names.push_back(kv.first);
    }
    return class_names;
  }

  std::unique_ptr<core::ObjectFactory> assign(const std::string &class_name) {
    auto mapping = file_mapping_.find(class_name);
    if (mapping != file_mapping_.end()) {
      return std::unique_ptr<core::ObjectFactory>(new PythonObjectFactory(mapping->second, mapping->first));
    }
    return nullptr;
  }

  std::map<std::string, std::string> file_mapping_;
};

#pragma GCC visibility pop

#endif /* EXTENSIONS_PYPROCESS_PYPROCCREATOR_H_ */

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
#include <vector>
#include <string>
#include <memory>

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

  std::shared_ptr<core::CoreComponent> create(const std::string &name) override {
    auto ptr = std::static_pointer_cast<minifi::python::processors::ExecutePythonProcessor>(DefautObjectFactory::create(name));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return std::static_pointer_cast<core::CoreComponent>(ptr);
  }

  std::shared_ptr<core::CoreComponent> create(const std::string &name, const utils::Identifier &uuid) override {
    auto ptr = std::static_pointer_cast<minifi::python::processors::ExecutePythonProcessor>(DefautObjectFactory::create(name, uuid));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return std::static_pointer_cast<core::CoreComponent>(ptr);
  }

  core::CoreComponent* createRaw(const std::string &name) override {
    auto ptr = dynamic_cast<minifi::python::processors::ExecutePythonProcessor*>(DefautObjectFactory::createRaw(name));
    ptr->initialize();
    ptr->setProperty(minifi::python::processors::ExecutePythonProcessor::ScriptFile, file_);
    return dynamic_cast<core::CoreComponent*>(ptr);
  }

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

#pragma GCC visibility pop

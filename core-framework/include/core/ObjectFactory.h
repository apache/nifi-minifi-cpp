// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <memory>
#include <utility>
#include "core/ClassName.h"
#include "minifi-cpp/core/ObjectFactory.h"

namespace org::apache::nifi::minifi::core {

class ObjectFactoryImpl : public ObjectFactory {
 public:
  explicit ObjectFactoryImpl(std::string module_name) : module_name_(std::move(module_name)) {}

  ObjectFactoryImpl() = default;

  std::string getModuleName() const override {
    return module_name_;
  }

 protected:
  std::string module_name_;
};

/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
template<class T>
class DefaultObjectFactory : public ObjectFactoryImpl {
 public:
  DefaultObjectFactory()
      : className(core::className<T>()) {
  }

  explicit DefaultObjectFactory(std::string module_name)
      : ObjectFactoryImpl(std::move(module_name)),
      className(core::className<T>()) {
  }

  /**
   * Create a unique pointer to a new processor.
   */
  std::unique_ptr<CoreComponent> create(const std::string &name) override {
    return std::make_unique<T>(name);
  }

  /**
   * Create a unique pointer to a new processor.
   */
  std::unique_ptr<CoreComponent> create(const std::string &name, const utils::Identifier &uuid) override {
    return std::make_unique<T>(name, uuid);
  }

  /**
   * Create a raw pointer to a new processor.
   */
  gsl::owner<CoreComponent*> createRaw(const std::string &name) override {
    const gsl::owner<T*> ptr = new T(name);
    if (const auto converted = dynamic_cast<CoreComponent*>(ptr)) {
      return converted;
    }
    delete ptr;
    return nullptr;
  }

  /**
   * Create a raw pointer to a new processor.
   */
  gsl::owner<CoreComponent*> createRaw(const std::string &name, const utils::Identifier &uuid) override {
    const gsl::owner<T*> ptr = new T(name, uuid);
    if (const auto converted = dynamic_cast<CoreComponent*>(ptr)) {
      return converted;
    }
    delete ptr;
    return nullptr;
  }

  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  std::string getClassName() override {
    return className;
  }

 protected:
  std::string className;
};

}  // namespace org::apache::nifi::minifi::core

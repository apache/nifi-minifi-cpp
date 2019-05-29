#pragma once

/**
 * @file SupportedProperty.h
 * ISupportedProperty, SupportedProperty, SuportedProperties classes declarations
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

#include <vector>
#include <set>
#include <codecvt>

#include "core/ProcessContext.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

struct ISupportedProperty {
  void virtual Init(const std::shared_ptr<core::ProcessContext>& context) = 0;
};

class SupportedProperties {
  std::vector<ISupportedProperty*> listISupportedProperty_;
  std::vector<core::Property*> listProperties_;

public:
  void init(const std::shared_ptr<core::ProcessContext>& context) {
    for (auto pProp : listISupportedProperty_) {
      pProp->Init(context);
    }
  }

  std::set<core::Property> getProperties() const  {
    std::set<core::Property> ret;
    for (auto pProperty : listProperties_) {
      ret.insert(*pProperty);
    }
    return ret;
  }

  SupportedProperties() {}

  template <typename Arg, typename ...Args>
  SupportedProperties(Arg& arg, Args&... args) {
    add(arg, args...);
  }

private:
  template <typename Arg>
  void add(Arg& arg) {
    listISupportedProperty_.emplace_back(&arg);
    listProperties_.emplace_back(&arg);
  }

  template <typename Arg, typename ...Args>
  void add(Arg& arg, Args&... args) {
    add(arg);
    add(args...);
  }
};

template <typename T>
class SupportedProperty : public ISupportedProperty, public core::Property {
  T t_;

public:
  template <typename ...Args>
  SupportedProperty(const Args& ...args): core::Property(args...) {
  }

  void Init(const std::shared_ptr<core::ProcessContext>& context) override {
    context->getProperty(getName(), t_);
  }

  T value() {
    return t_;
  }
};

void SupportedProperty<std::wstring>::Init(const std::shared_ptr<core::ProcessContext>& context) {
  std::string val;
  context->getProperty(getName(), val);

  t_ = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(val);
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

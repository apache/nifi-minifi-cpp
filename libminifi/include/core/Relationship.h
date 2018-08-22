/**
 * @file Relationship.h
 * Relationship class declaration
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
#ifndef __RELATIONSHIP_H__
#define __RELATIONSHIP_H__

#include <string>
#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// undefined relationship for remote process group outgoing port and root process group incoming port
#define UNDEFINED_RELATIONSHIP "undefined"

inline bool isRelationshipNameUndefined(std::string name) {
  if (name == UNDEFINED_RELATIONSHIP)
    return true;
  else
    return false;
}

// Relationship Class
class Relationship {

 public:
  /*
   * Create a new relationship 
   */
  Relationship(const std::string name, const std::string description)
      : name_(name),
        description_(description) {
  }

  Relationship(const Relationship &other)
      : name_(other.name_),
        description_(other.description_) {
  }

  Relationship()
      : name_(UNDEFINED_RELATIONSHIP) {
  }
  // Destructor
  ~Relationship() {
  }
  // Get Name for the relationship
  std::string getName() const {
    return name_;
  }
  // Get Description for the relationship
  std::string getDescription() const {
    return description_;
  }
  // Compare
  bool operator <(const Relationship & right) const {
    return name_ < right.name_;
  }

  Relationship &operator=(const Relationship &other) {
    name_ = other.name_;
    description_ = other.description_;
    return *this;
  }
  // Whether it is a undefined relationship
  bool isRelationshipUndefined() {
    return isRelationshipNameUndefined(name_);
  }

 protected:

  // Name
  std::string name_;
  // Description
  std::string description_;

 private:
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif

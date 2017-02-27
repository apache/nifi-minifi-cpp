/*
 * Core.cpp
 *
 *  Created on: Mar 10, 2017
 *      Author: mparisi
 */

#include "core/core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// Set UUID
void CoreComponent::setUUID(uuid_t uuid) {
  uuid_copy(uuid_, uuid);
  char uuidStr[37];
  uuid_unparse_lower(uuid_, uuidStr);
  uuidStr_ = uuidStr;
}
// Get UUID
bool CoreComponent::getUUID(uuid_t uuid) const {
  if (uuid) {
    uuid_copy(uuid, uuid_);
    return true;
  } else {
    return false;
  }
}

// Get UUID
unsigned const char *CoreComponent::getUUID() const {
  return uuid_;
}

// Set Processor Name
void CoreComponent::setName(const std::string name) {
  name_ = name;

}
// Get Process Name
std::string CoreComponent::getName() const {
  return name_;
}
}
}
}
}
}

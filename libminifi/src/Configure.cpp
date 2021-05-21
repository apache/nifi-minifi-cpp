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

#include "properties/Configure.h"

#include "utils/gsl.h"

#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

bool Configure::get(const std::string& key, std::string& value) const {
  bool found = getString(key, value);
  if (decryptor_ && found && isEncrypted(key)) {
    value = decryptor_->decrypt(value);
  }
  return found;
}

bool Configure::get(const std::string& key, const std::string& alternate_key, std::string& value) const {
  if (get(key, value)) {
    if (get(alternate_key)) {
      const auto logger = logging::LoggerFactory<Configure>::getLogger();
      logger->log_warn("Both the property '%s' and an alternate property '%s' are set. Using '%s'.", key, alternate_key, key);
    }
    return true;
  } else if (get(alternate_key, value)) {
    const auto logger = logging::LoggerFactory<Configure>::getLogger();
    logger->log_warn("%s is an alternate property that may not be supported in future releases. Please use %s instead.", alternate_key, key);
    return true;
  } else {
    return false;
  }
}

utils::optional<std::string> Configure::get(const std::string& key) const {
  utils::optional<std::string> value = getString(key);
  if (decryptor_ && value && isEncrypted(key)) {
    return decryptor_->decrypt(*value);
  } else {
    return value;
  }
}

bool Configure::isEncrypted(const std::string& key) const {
  gsl_Expects(decryptor_);
  utils::optional<std::string> encryption_marker = getString(key + ".protected");
  return decryptor_->isValidEncryptionMarker(encryption_marker);
}

utils::optional<std::string> Configure::getAgentClass() const {
  std::string agent_class;
  if (get("nifi.c2.agent.class", "c2.agent.class", agent_class) && !agent_class.empty()) {
    return agent_class;
  }
  return {};
}

std::string Configure::getAgentIdentifier() const {
  std::string agent_id;
  if (!get("nifi.c2.agent.identifier", "c2.agent.identifier", agent_id) || agent_id.empty()) {
    std::lock_guard<std::mutex> guard(fallback_identifier_mutex_);
    return fallback_identifier_;
  }
  return agent_id;
}

void Configure::setFallbackAgentIdentifier(const std::string& id) {
  std::lock_guard<std::mutex> guard(fallback_identifier_mutex_);
  fallback_identifier_ = id;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

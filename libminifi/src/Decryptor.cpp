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
#include "properties/Decryptor.h"

#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

void Decryptor::decryptSensitiveProperties(Configure& configure) const {
  std::shared_ptr<minifi::core::logging::Logger> logger = logging::LoggerFactory<Decryptor>::getLogger();
  logger->log_info("Decrypting sensitive properties...");
  int num_properties_decrypted = 0;

  for (const auto& property_key : configure.getConfiguredKeys()) {
    const std::string property_value = configure.get(property_key).value();

    utils::optional<std::string> encryption_marker = configure.get(property_key + ".protected");
    if (Decryptor::isValidEncryptionMarker(encryption_marker)) {
      std::string decrypted_property_value;
      try {
        decrypted_property_value = decrypt(property_value);
      } catch (const std::exception& ex) {
        logger->log_error("Could not decrypt property %s; error: %s", property_key, ex.what());
        continue;
      }
      configure.set(property_key, decrypted_property_value);
      logger->log_info("Decrypted property: %s", property_key);
      ++num_properties_decrypted;
    }
  }

  logger->log_info("Finished decrypting %d sensitive properties.", num_properties_decrypted);
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

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
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <set>

#include "ConvertUpdate.h"
#include "utils/HTTPClient.h"
#include "io/BaseStream.h"
#include "io/BufferStream.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ConvertUpdate::SSLContext("SSL Context Service", "The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.", "");

void ConvertUpdate::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession>& /*session*/) {
  if (nullptr == mqtt_service_) {
    context->yield();
    return;
  }
  std::vector<uint8_t> update;
  bool received_update = false;
  while (mqtt_service_->get(100, listening_topic, update)) {
    // first we have the input topic string followed by the update URI
    if (!update.empty()) {
      io::BufferStream stream(update.data(), update.size());

      std::string returnTopic, url;

      if (returnTopic.empty() || url.empty()) {
        logger_->log_debug("topic and/or URL are empty");
        break;
      }

      stream.read(returnTopic);
      stream.read(url);

      /**
       * Not having curl support is actually okay for MQTT to be built, but running the update processor requires
       * that we have curl available.
       */
      auto client_ptr = core::ClassLoader::getDefaultClassLoader().instantiateRaw("HTTPClient", "HTTPClient");
      if (nullptr == client_ptr) {
        logger_->log_error("Could not locate HTTPClient. You do not have cURL support!");
        return;
      }
      std::unique_ptr<utils::BaseHTTPClient> client = std::unique_ptr<utils::BaseHTTPClient>(dynamic_cast<utils::BaseHTTPClient*>(client_ptr));
      client->initialize("GET");
      client->setConnectionTimeout(std::chrono::milliseconds(2000));
      client->setReadTimeout(std::chrono::milliseconds(2000));

      if (client->submit()) {
        auto data = client->getResponseBody();
        std::vector<uint8_t> raw_data;
        std::transform(std::begin(data), std::end(data), std::back_inserter(raw_data), [](char c) {
          return (uint8_t)c;
        });
        mqtt_service_->send(returnTopic, raw_data);
      }

      received_update = true;
    } else {
      break;
    }
  }

  if (!received_update) {
    context->yield();
  }
}

void ConvertUpdate::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(MQTTControllerService);
  properties.insert(ListeningTopic);
  properties.insert(SSLContext);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

REGISTER_INTERNAL_RESOURCE(ConvertUpdate);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

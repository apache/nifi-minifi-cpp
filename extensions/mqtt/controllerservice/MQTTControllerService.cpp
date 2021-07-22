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

#include "MQTTControllerService.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <memory>
#include <set>
#include "core/Property.h"
#include "core/Resource.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property MQTTControllerService::BrokerURL("Broker URI", "The URI to use to connect to the MQTT broker", "");
core::Property MQTTControllerService::ClientID("Client ID", "MQTT client ID to use", "");
core::Property MQTTControllerService::UserName("Username", "Username to use when connecting to the broker", "");
core::Property MQTTControllerService::Password("Password", "Password to use when connecting to the broker", "");
core::Property MQTTControllerService::KeepLiveInterval("Keep Alive Interval", "Defines the maximum time interval between messages sent or received", "60 sec");
core::Property MQTTControllerService::ConnectionTimeOut("Connection Timeout", "Maximum time interval the client will wait for the network connection to the MQTT server", "30 sec");
core::Property MQTTControllerService::QOS("Quality of Service", "The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'", "MQTT_QOS_0");
core::Property MQTTControllerService::Topic("Topic", "The topic to publish the message to", "");
core::Property MQTTControllerService::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");

void MQTTControllerService::initialize() {
  if (initialized_)
    return;

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

void MQTTControllerService::onEnable() {
  for (auto &linked_service : linked_services_) {
    std::shared_ptr<controllers::SSLContextService> ssl_service = std::dynamic_pointer_cast<controllers::SSLContextService>(linked_service);
    if (nullptr != ssl_service) {
      // security is enabled.
      ssl_context_service_ = ssl_service;
    }
  }
  if (getProperty(BrokerURL.getName(), uri_) && getProperty(ClientID.getName(), clientID_)) {
    if (!client_) {
      MQTTClient_create(&client_, uri_.c_str(), clientID_.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    }

    if (client_) {
      MQTTClient_setCallbacks(client_, this, reconnectCallback, receiveCallback, deliveryCallback);
      // call reconnect to bootstrap
      this->reconnect();
    }
  }
}

void MQTTControllerService::initializeProperties() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(BrokerURL);
  supportedProperties.insert(ClientID);
  supportedProperties.insert(UserName);
  supportedProperties.insert(Password);

  supportedProperties.insert(KeepLiveInterval);
  supportedProperties.insert(ConnectionTimeOut);
  supportedProperties.insert(Topic);
  supportedProperties.insert(QOS);
  supportedProperties.insert(SecurityProtocol);
  setSupportedProperties(supportedProperties);
}

REGISTER_INTERNAL_RESOURCE_AS(MQTTControllerService, ("MQTTContextService"));

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

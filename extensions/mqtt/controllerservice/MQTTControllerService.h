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

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <iostream>
#include <memory>
#include <vector>
#include <utility>
#include <map>
#include <string>

#include "utils/StringUtils.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "controllers/SSLContextService.h"
#include "concurrentqueue.h"
#include "MQTTClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

static constexpr const char* const MQTT_QOS_0 = "0";
static constexpr const char* const MQTT_QOS_1 = "1";
static constexpr const char* const MQTT_QOS_2 = "2";

class Message {
 public:
  // empty constructor facilitates moves
  Message() = default;
  explicit Message(const std::string &topic, void *data, size_t dataLen)
      : topic_(topic),
        data_(reinterpret_cast<uint8_t*>(data), (reinterpret_cast<uint8_t*>(data) + dataLen)) {
  }

  Message(const Message &other) = default;
  Message(Message &&other) = default;

  ~Message() = default;

  Message &operator=(const Message &other) = default;
  Message &operator=(Message &&other) = default;

  std::string topic_;
  std::vector<uint8_t> data_;
};

/**
 * MQTTContextService provides a controller service for MQTT connectivity.
 *
 */
class MQTTControllerService : public core::controller::ControllerService {
 public:
  explicit MQTTControllerService(const std::string &name, const utils::Identifier &uuid = {})
      : ControllerService(name, uuid),
        initialized_(false),
        client_(nullptr),
        keepAliveInterval_(0),
        connectionTimeOut_(0),
        qos_(2),
        ssl_context_service_(nullptr),
        logger_(logging::LoggerFactory<MQTTControllerService>::getLogger()) {
  }

  explicit MQTTControllerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        initialized_(false),
        client_(nullptr),
        keepAliveInterval_(0),
        connectionTimeOut_(0),
        qos_(2),
        ssl_context_service_(nullptr),
        logger_(logging::LoggerFactory<MQTTControllerService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  static core::Property BrokerURL;
  static core::Property ClientID;
  static core::Property UserName;
  static core::Property Password;
  static core::Property CleanSession;
  static core::Property KeepLiveInterval;
  static core::Property ConnectionTimeOut;
  static core::Property Topic;
  static core::Property QOS;
  static core::Property SecurityProtocol;

  virtual void initialize();

  void yield() {
  }

  int send(const std::string &topic, const std::vector<uint8_t> &data) {
    int token;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    const uint8_t *d = data.data();
    pubmsg.payload = const_cast<uint8_t*>(d);
    pubmsg.payloadlen = data.size();
    pubmsg.qos = qos_;
    pubmsg.retained = 0;

    auto resp = MQTTClient_publishMessage(client_, topic.c_str(), &pubmsg, &token);
    if (resp != MQTTCLIENT_SUCCESS) {
      return -1;
    }
    if (qos_ == 0) {
      std::unique_lock<std::mutex> lock(delivery_mutex_);
      delivered_[token] = true;
    }
    return token;
  }

  int send(const std::string &topic, const uint8_t *data, size_t dataSize) {
    int token;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = const_cast<uint8_t*>(data);
    pubmsg.payloadlen = dataSize;
    pubmsg.qos = qos_;
    pubmsg.retained = 0;

    auto resp = MQTTClient_publishMessage(client_, topic.c_str(), &pubmsg, &token);
    if (resp != MQTTCLIENT_SUCCESS) {
      return -1;
    }

    if (qos_ == 0) {
      std::unique_lock<std::mutex> lock(delivery_mutex_);
      delivered_[token] = true;
    }
    return token;
  }

  bool isRunning() {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() {
    return false;
  }

  virtual void onEnable();

  void subscribeToTopic(const std::string newTopic) {
    std::lock_guard<std::mutex> lock(initialization_mutex_);
    if (topics_.find(newTopic) == topics_.end()) {
      MQTTClient_subscribe(client_, newTopic.c_str(), qos_);
      topics_[newTopic].size_approx();
    }
  }

  bool waitForDelivery(const uint64_t millisToWait, int token) {
    std::unique_lock<std::mutex> lock(delivery_mutex_);
    if (delivery_notification_.wait_for(lock, std::chrono::milliseconds(millisToWait), [&] {return delivered_[token] == true;})) {
      bool delivered = delivered_[token];
      delivered_.erase(token);
      return delivered;
    } else {
      delivered_.erase(token);
      return false;
    }
  }

  bool get(const uint64_t millisToWait, const std::string &topic, std::vector<uint8_t> &data) {
    std::unique_lock<std::mutex> lock(delivery_mutex_);
    if (delivery_notification_.wait_for(lock, std::chrono::milliseconds(millisToWait), [&] {return topics_[topic].size_approx() > 0;})) {
      Message resp;
      if (topics_[topic].try_dequeue(resp)) {
        data = std::move(resp.data_);
        return true;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  bool awaitResponse(const uint64_t millisToWait, int token, const std::string &topic, std::vector<uint8_t> &data) {
    std::unique_lock<std::mutex> lock(delivery_mutex_);
    if (delivery_notification_.wait_for(lock, std::chrono::milliseconds(millisToWait), [&] {
      return
      delivered_[token] == true;
    })) {
      bool delivered = delivered_[token];
      if (delivered) {
        if (delivery_notification_.wait_for(lock, std::chrono::milliseconds(millisToWait), [&] {return topics_[topic].size_approx() > 0;})) {
          Message resp;
          if (topics_[topic].try_dequeue(resp)) {
            data = std::move(resp.data_);
            return true;
          } else {
            return false;
          }
        } else {
          return false;
        }
      }
      delivered_.erase(token);
      return delivered;
    } else {
      delivered_.erase(token);
      return false;
    }
  }

 protected:
  void acknowledgeDelivery(MQTTClient_deliveryToken token) {
    std::lock_guard<std::mutex> lock(delivery_mutex_);
    // locked the mutex
    auto finder = delivered_.find(token);
    // only acknowledge delivery if we expect the delivery to occur, otherwise
    // we won't have any waiters.
    if (finder != delivered_.end()) {
      delivered_[token] = true;
    }
  }

  void enqueue(const std::string &topic, Message &&message) {
    std::unique_lock<std::mutex> lock(delivery_mutex_);
    topics_[topic].enqueue(std::move(message));
    delivery_notification_.notify_one();
  }

  static void deliveryCallback(void *context, MQTTClient_deliveryToken dt) {
    MQTTControllerService *service = reinterpret_cast<MQTTControllerService *>(context);
    service->acknowledgeDelivery(dt);
  }

  static int receiveCallback(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    MQTTControllerService *service = reinterpret_cast<MQTTControllerService *>(context);
    std::string topic(topicName, topicLen == 0 ? strlen(topicName) : topicLen);
    Message queueMessage(topic, message->payload, message->payloadlen);
    service->enqueue(topic, std::move(queueMessage));
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
  }
  static void reconnectCallback(void *context, char* /*cause*/) {
    MQTTControllerService *service = reinterpret_cast<MQTTControllerService *>(context);
    service->reconnect();
  }

  bool reconnect() {
    if (!client_)
      return false;
    if (MQTTClient_isConnected(client_))
      return true;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = keepAliveInterval_;
    conn_opts.cleansession = 1;
    if (!userName_.empty()) {
      conn_opts.username = userName_.c_str();
      conn_opts.password = passWord_.c_str();
    }
    if (ssl_context_service_ != nullptr)
      conn_opts.ssl = &sslopts_;
    if (MQTTClient_connect(client_, &conn_opts) != MQTTCLIENT_SUCCESS) {
      logger_->log_error("Failed to connect to MQTT broker %s", uri_);
      return false;
    }

    if (!topic_.empty()) {
      std::unique_lock<std::mutex> lock(delivery_mutex_);
      MQTTClient_subscribe(client_, topic_.c_str(), qos_);
    }
    return true;
  }

  virtual void initializeProperties();

  std::mutex initialization_mutex_;
  std::atomic<bool> initialized_;

  MQTTClient client_;
  std::string uri_;
  std::string topic_;
  int64_t keepAliveInterval_;
  int64_t connectionTimeOut_;
  int64_t qos_;
  std::string clientID_;
  std::string userName_;
  std::string passWord_;

 private:
  std::map<int, bool> delivered_;
  std::map<std::string, moodycamel::ConcurrentQueue<Message> > topics_;

  std::mutex delivery_mutex_;
  std::condition_variable delivery_notification_;

  MQTTClient_SSLOptions sslopts_;

  std::shared_ptr<controllers::SSLContextService> ssl_context_service_;

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

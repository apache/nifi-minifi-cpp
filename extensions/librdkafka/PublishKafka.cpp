/**
 * @file PublishKafka.cpp
 * PublishKafka class implementation
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
#include "PublishKafka.h"
#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <set>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property PublishKafka::SeedBrokers(
    core::PropertyBuilder::createProperty("Known Brokers")->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>")->isRequired(true)->supportsExpressionLanguage(
        true)->build());

core::Property PublishKafka::Topic(core::PropertyBuilder::createProperty("Topic Name")->withDescription("The Kafka Topic of interest")->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property PublishKafka::DeliveryGuarantee(
    core::PropertyBuilder::createProperty("Delivery Guarantee")->withDescription("TSpecifies the requirement for guaranteeing that a message is sent to Kafka")->isRequired(false)
        ->supportsExpressionLanguage(true)->withDefaultValue("DELIVERY_ONE_NODE")->build());

core::Property PublishKafka::MaxMessageSize(core::PropertyBuilder::createProperty("Max Request Size")->withDescription("Maximum Kafka protocol request message size")->isRequired(false)->build());

core::Property PublishKafka::RequestTimeOut(
    core::PropertyBuilder::createProperty("Request Timeout")->withDescription("The ack timeout of the producer request in milliseconds")->isRequired(false)->supportsExpressionLanguage(true)->build());

core::Property PublishKafka::ClientName(
    core::PropertyBuilder::createProperty("Client Name")->withDescription("Client Name to use when communicating with Kafka")->isRequired(true)->supportsExpressionLanguage(true)->build());

/**
 * These needn's have EL support.
 */
core::Property PublishKafka::BatchSize("Batch Size", "Maximum number of messages batched in one MessageSet", "");
core::Property PublishKafka::AttributeNameRegex("Attributes to Send as Headers", "Any attribute whose name matches the regex will be added to the Kafka messages as a Header", "");
core::Property PublishKafka::QueueBufferMaxTime("Queue Buffering Max Time", "Delay to wait for messages in the producer queue to accumulate before constructing message batches", "");
core::Property PublishKafka::QueueBufferMaxSize("Queue Max Buffer Size", "Maximum total message size sum allowed on the producer queue", "");
core::Property PublishKafka::QueueBufferMaxMessage("Queue Max Message", "Maximum number of messages allowed on the producer queue", "");
core::Property PublishKafka::CompressCodec("Compress Codec", "compression codec to use for compressing message sets", COMPRESSION_CODEC_NONE);
core::Property PublishKafka::MaxFlowSegSize("Max Flow Segment Size", "Maximum flow content payload segment size for the kafka record", "");
core::Property PublishKafka::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
core::Property PublishKafka::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
core::Property PublishKafka::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
core::Property PublishKafka::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
core::Property PublishKafka::SecurityPrivateKeyPassWord("Security Pass Phrase", "Private key passphrase", "");
core::Property PublishKafka::KerberosServiceName("Kerberos Service Name", "Kerberos Service Name", "");
core::Property PublishKafka::KerberosPrincipal("Kerberos Principal", "Keberos Principal", "");
core::Property PublishKafka::KerberosKeytabPath("Kerberos Keytab Path",
                                                "The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.", "");
core::Property PublishKafka::MessageKeyField("Message Key Field", "The name of a field in the Input Records that should be used as the Key for the Kafka message.\n"
                                             "Supports Expression Language: true (will be evaluated using flow file attributes)",
                                             "");
core::Relationship PublishKafka::Success("success", "Any FlowFile that is successfully sent to Kafka will be routed to this Relationship");
core::Relationship PublishKafka::Failure("failure", "Any FlowFile that cannot be sent to Kafka will be routed to this Relationship");

void PublishKafka::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(SeedBrokers);
  properties.insert(Topic);
  properties.insert(DeliveryGuarantee);
  properties.insert(MaxMessageSize);
  properties.insert(RequestTimeOut);
  properties.insert(ClientName);
  properties.insert(AttributeNameRegex);
  properties.insert(BatchSize);
  properties.insert(QueueBufferMaxTime);
  properties.insert(QueueBufferMaxSize);
  properties.insert(QueueBufferMaxMessage);
  properties.insert(CompressCodec);
  properties.insert(MaxFlowSegSize);
  properties.insert(SecurityProtocol);
  properties.insert(SecurityCA);
  properties.insert(SecurityCert);
  properties.insert(SecurityPrivateKey);
  properties.insert(SecurityPrivateKeyPassWord);
  properties.insert(KerberosServiceName);
  properties.insert(KerberosPrincipal);
  properties.insert(KerberosKeytabPath);
  properties.insert(MessageKeyField);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void PublishKafka::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {

}

bool PublishKafka::configureNewConnection(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &ff) {
  std::string value;
  int64_t valInt;
  std::string valueConf;
  char errstr[512];
  rd_kafka_conf_res_t result;

  auto conf_ = rd_kafka_conf_new();

  //auto topic_conf_ = rd_kafka_topic_conf_new();

  auto key = conn->getKey();

  if (!key->brokers_.empty()) {
    result = rd_kafka_conf_set(conf_, "bootstrap.servers", key->brokers_.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: bootstrap.servers [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  } else {
    logger_->log_error("There are no brokers");
    return false;
  }

  if (!key->client_id_.empty()) {
    rd_kafka_conf_set(conf_, "client.id", key->client_id_.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: client.id [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  } else {
    logger_->log_error("Client id is empty");
    return false;
  }

// Kerberos configuration
  if (context->getProperty(KerberosServiceName.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.service.name", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: sasl.kerberos.service.name [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(KerberosPrincipal.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.principal", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: sasl.kerberos.principal [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(KerberosKeytabPath.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.keytab", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: sasl.kerberos.keytab [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }

  value = "";
  if (context->getProperty(MaxMessageSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    valueConf = std::to_string(valInt);
    result = rd_kafka_conf_set(conf_, "message.max.bytes", valueConf.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: message.max.bytes [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(QueueBufferMaxMessage.getName(), value) && !value.empty()) {
    rd_kafka_conf_set(conf_, "queue.buffering.max.messages", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: queue.buffering.max.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(AttributeNameRegex.getName(), value) && !value.empty()) {
    attributeNameRegex.assign(value);
    logger_->log_debug("PublishKafka: AttributeNameRegex %s", value);
  }
  value = "";
  if (context->getProperty(QueueBufferMaxSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    valInt = valInt / 1024;
    valueConf = std::to_string(valInt);
    rd_kafka_conf_set(conf_, "queue.buffering.max.kbytes", valueConf.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: queue.buffering.max.kbytes [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  max_seg_size_ = ULLONG_MAX;
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    max_seg_size_ = valInt;
    logger_->log_debug("PublishKafka: max flow segment size [%d]", max_seg_size_);
  }
  value = "";
  if (context->getProperty(QueueBufferMaxTime.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      rd_kafka_conf_set(conf_, "queue.buffering.max.ms", valueConf.c_str(), errstr, sizeof(errstr));
      logger_->log_debug("PublishKafka: queue.buffering.max.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK)
        logger_->log_error("PublishKafka: configure queue buffer error result [%s]", errstr);
    }
  }
  value = "";
  if (context->getProperty(BatchSize.getName(), value) && !value.empty()) {
    rd_kafka_conf_set(conf_, "batch.num.messages", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: batch.num.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure batch size error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(CompressCodec.getName(), value) && !value.empty() && value != "none") {
    rd_kafka_conf_set(conf_, "compression.codec", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: compression.codec [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure compression codec error result [%s]", errstr);
  }
  value = "";
  value = "";
  if (context->getProperty(SecurityProtocol.getName(), value) && !value.empty()) {
    if (value == SECURITY_PROTOCOL_SSL) {
      rd_kafka_conf_set(conf_, "security.protocol", value.c_str(), errstr, sizeof(errstr));
      logger_->log_debug("PublishKafka: security.protocol [%s]", value);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      } else {
        value = "";
        if (context->getProperty(SecurityCA.getName(), value) && !value.empty()) {
          rd_kafka_conf_set(conf_, "ssl.ca.location", value.c_str(), errstr, sizeof(errstr));
          logger_->log_debug("PublishKafka: ssl.ca.location [%s]", value);
          if (result != RD_KAFKA_CONF_OK)
            logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        }
        value = "";
        if (context->getProperty(SecurityCert.getName(), value) && !value.empty()) {
          rd_kafka_conf_set(conf_, "ssl.certificate.location", value.c_str(), errstr, sizeof(errstr));
          logger_->log_debug("PublishKafka: ssl.certificate.location [%s]", value);
          if (result != RD_KAFKA_CONF_OK)
            logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        }
        value = "";
        if (context->getProperty(SecurityPrivateKey.getName(), value) && !value.empty()) {
          rd_kafka_conf_set(conf_, "ssl.key.location", value.c_str(), errstr, sizeof(errstr));
          logger_->log_debug("PublishKafka: ssl.key.location [%s]", value);
          if (result != RD_KAFKA_CONF_OK)
            logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        }
        value = "";
        if (context->getProperty(SecurityPrivateKeyPassWord.getName(), value) && !value.empty()) {
          rd_kafka_conf_set(conf_, "ssl.key.password", value.c_str(), errstr, sizeof(errstr));
          logger_->log_debug("PublishKafka: ssl.key.password [%s]", value);
          if (result != RD_KAFKA_CONF_OK)
            logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        }
      }
    }
  }

// Add all of the dynamic properties as librdkafka configurations
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("PublishKafka registering %d librdkafka dynamic properties", dynamic_prop_keys.size());

  for (const auto &key : dynamic_prop_keys) {
    value = "";
    if (context->getDynamicProperty(key, value) && !value.empty()) {
      logger_->log_debug("PublishKafka: DynamicProperty: [%s] -> [%s]", key, value);
      rd_kafka_conf_set(conf_, key.c_str(), value.c_str(), errstr, sizeof(errstr));
    } else {
      logger_->log_warn("PublishKafka Dynamic Property '%s' is empty and therefore will not be configured", key);
    }
  }

  auto producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf_, errstr, sizeof(errstr));

  if (!producer) {
    logger_->log_error("Failed to create Kafka producer %s", errstr);
    return false;
  }

  conn->setConnection(producer, conf_);

  return true;
}

void PublishKafka::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("Enter trigger");
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  std::string client_id, brokers, topic;

  std::shared_ptr<KafkaConnection> conn = nullptr;
// get the client ID, brokers, and topic from either the flowfile, the configuration, or the properties
  if (context->getProperty(ClientName, client_id, flowFile) && context->getProperty(SeedBrokers, brokers, flowFile) && context->getProperty(Topic, topic, flowFile)) {
    KafkaConnectionKey key;
    key.brokers_ = brokers;
    key.client_id_ = client_id;

    conn = connection_pool_.getOrCreateConnection(key);

    if (!conn->initialized()) {

      logger_->log_trace("Connection not initialized to %s, %s, %s", client_id, brokers, topic);
      // get the ownership so we can configure this connection
      KafkaLease lease = conn->obtainOwnership();

      if (!configureNewConnection(conn, context, flowFile)) {
        logger_->log_error("Could not configure Kafka Connection");
        session->transfer(flowFile, Failure);
        return;
      }

      // lease will go away
    }

    if (!conn->hasTopic(topic)) {

      auto topic_conf_ = rd_kafka_topic_conf_new();
      auto topic_reference = rd_kafka_topic_new(conn->getConnection(), topic.c_str(), topic_conf_);
      rd_kafka_conf_res_t result;
      std::string value;
      char errstr[512];
      int64_t valInt;
      std::string valueConf;

      if (context->getProperty(DeliveryGuarantee, value, flowFile) && !value.empty()) {
        rd_kafka_topic_conf_set(topic_conf_, "request.required.acks", value.c_str(), errstr, sizeof(errstr));
        logger_->log_debug("PublishKafka: request.required.acks [%s]", value);
        if (result != RD_KAFKA_CONF_OK)
          logger_->log_error("PublishKafka: configure delivery guarantee error result [%s]", errstr);
      }
      value = "";
      if (context->getProperty(RequestTimeOut, value, flowFile) && !value.empty()) {
        core::TimeUnit unit;
        if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
          valueConf = std::to_string(valInt);
          rd_kafka_topic_conf_set(topic_conf_, "request.timeout.ms", valueConf.c_str(), errstr, sizeof(errstr));
          logger_->log_debug("PublishKafka: request.timeout.ms [%s]", valueConf);
          if (result != RD_KAFKA_CONF_OK)
            logger_->log_error("PublishKafka: configure timeout error result [%s]", errstr);
        }
      }

      std::shared_ptr<KafkaTopic> kafkaTopicref = std::make_shared<KafkaTopic>(topic_reference, topic_conf_);

      conn->putTopic(topic, kafkaTopicref);
    }
    /*rd_kafka_conf_set(conf_, "client.id", client_id.c_str(), errstr, sizeof(errstr));
     logger_->log_debug("PublishKafka: client.id [%s]", value);
     if (result != RD_KAFKA_CONF_OK)
     logger_->log_error("PublishKafka: configure error result [%s]", errstr);
     */
  } else {
    logger_->log_error("Do not have required properties");
    session->transfer(flowFile, Failure);
    return;
  }

  std::string kafkaKey;
  kafkaKey = "";
  if (context->getDynamicProperty(MessageKeyField, kafkaKey, flowFile) && !kafkaKey.empty()) {
    logger_->log_debug("PublishKafka: Message Key Field [%s]", kafkaKey);
  } else {
    kafkaKey = flowFile->getUUIDStr();
  }

  auto thisTopic = conn->getTopic(topic);
  if (thisTopic) {
    PublishKafka::ReadCallback callback(max_seg_size_, kafkaKey, thisTopic->getTopic(), conn->getConnection(), flowFile, attributeNameRegex);
    session->read(flowFile, &callback);
    if (callback.status_ < 0) {
      logger_->log_error("Failed to send flow to kafka topic %s", topic);
      session->transfer(flowFile, Failure);
    } else {
      logger_->log_debug("Sent flow with length %d to kafka topic %s", callback.read_size_, topic);
      session->transfer(flowFile, Success);
    }
  } else {
    logger_->log_error("Topic %s is invalid", topic);
    session->transfer(flowFile, Failure);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

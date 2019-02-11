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

core::Property PublishKafka::SeedBrokers("Known Brokers", "A comma-separated list of known Kafka Brokers in the format <host>:<port>", "");
core::Property PublishKafka::Topic("Topic Name", "The Kafka Topic of interest", "");
core::Property PublishKafka::DeliveryGuarantee("Delivery Guarantee", "Specifies the requirement for guaranteeing that a message is sent to Kafka", DELIVERY_ONE_NODE);
core::Property PublishKafka::MaxMessageSize("Max Request Size", "Maximum Kafka protocol request message size", "");
core::Property PublishKafka::RequestTimeOut("Request Timeout", "The ack timeout of the producer request in milliseconds", "");
core::Property PublishKafka::ClientName("Client Name", "Client Name to use when communicating with Kafka", "");
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
core::Property PublishKafka::KerberosKeytabPath("Kerberos Keytab Path", "The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.", "");
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
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void PublishKafka::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;
  int64_t valInt;
  std::string valueConf;
  char errstr[512];
  rd_kafka_conf_res_t result;

  conf_ = rd_kafka_conf_new();
  topic_conf_ = rd_kafka_topic_conf_new();


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
  if (context->getProperty(SeedBrokers.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "bootstrap.servers", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: bootstrap.servers [%s]", value);
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
  if (context->getProperty(ClientName.getName(), value) && !value.empty()) {
    rd_kafka_conf_set(conf_, "client.id", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: client.id [%s]", value);
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
      valInt = valInt/1024;
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
        logger_->log_error("PublishKafka: configure error result [%s]", errstr);
    }
  }
  value = "";
  if (context->getProperty(BatchSize.getName(), value) && !value.empty()) {
    rd_kafka_conf_set(conf_, "batch.num.messages", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: batch.num.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(CompressCodec.getName(), value) && !value.empty()) {
    rd_kafka_conf_set(conf_, "compression.codec", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: compression.codec [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(DeliveryGuarantee.getName(), value) && !value.empty()) {
    rd_kafka_topic_conf_set(topic_conf_, "request.required.acks", value.c_str(), errstr, sizeof(errstr));
    logger_->log_debug("PublishKafka: request.required.acks [%s]", value);
    if (result != RD_KAFKA_CONF_OK)
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
  }
  value = "";
  if (context->getProperty(RequestTimeOut.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      rd_kafka_topic_conf_set(topic_conf_, "request.timeout.ms", valueConf.c_str(), errstr, sizeof(errstr));
      logger_->log_debug("PublishKafka: request.timeout.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK)
        logger_->log_error("PublishKafka: configure error result [%s]", errstr);
    }
  }
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
  value = "";
  if (context->getProperty(Topic.getName(), value) && !value.empty()) {
    topic_ = value;
    logger_->log_debug("PublishKafka: topic [%s]", topic_);
  } else {
    logger_->log_warn("PublishKafka: topic not configured");
    return;
  }

    // Add all of the dynamic properties as librdkafka configurations
    const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
    logger_->log_info("PublishKafka registering %d librdkafka dynamic properties", dynamic_prop_keys.size());

    for (const auto &key : dynamic_prop_keys) {
        value = "";
        if (context->getDynamicProperty(key, value) && !value.empty()) {
            logger_->log_debug("PublishKafka: DynamicProperty -> [%s]", value);
            rd_kafka_conf_set(conf_, key.c_str(), value.c_str(), errstr, sizeof(errstr));
        } else {
            logger_->log_warn("PublishKafka Dynamic Property '%s' is empty and therefore will not be configured", key);
        }
    }

  rk_= rd_kafka_new(RD_KAFKA_PRODUCER, conf_,
            errstr, sizeof(errstr));

  if (!rk_) {
    logger_->log_error("Failed to create Kafka producer %s", errstr);
    return;
  }

  rkt_ = rd_kafka_topic_new(rk_, topic_.c_str(), topic_conf_);

  if (!rkt_) {
    logger_->log_error("Failed to create topic %s", errstr);
    return;
  }

}

void PublishKafka::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  if (!rk_ || !rkt_) {
    session->transfer(flowFile, Failure);
    return;
  }

  std::string kafkaKey = flowFile->getUUIDStr();;
  std::string value;

  if (flowFile->getAttribute(KAFKA_KEY_ATTRIBUTE, value))
    kafkaKey = value;

  PublishKafka::ReadCallback callback(max_seg_size_, kafkaKey, rkt_, rk_, flowFile, attributeNameRegex);
  session->read(flowFile, &callback);
  if (callback.status_ < 0) {
    logger_->log_error("Failed to send flow to kafka topic %s", topic_);
    session->transfer(flowFile, Failure);
  } else {
    logger_->log_debug("Sent flow with length %d to kafka topic %s", callback.read_size_, topic_);
    session->transfer(flowFile, Success);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

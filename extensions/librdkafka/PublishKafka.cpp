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
#include "utils/ScopeGuard.h"
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
    core::PropertyBuilder::createProperty("Delivery Guarantee")->withDescription("Specifies the requirement for guaranteeing that a message is sent to Kafka")->isRequired(false)
        ->supportsExpressionLanguage(true)->withDefaultValue(DELIVERY_ONE_NODE)->build());

core::Property PublishKafka::MaxMessageSize(core::PropertyBuilder::createProperty("Max Request Size")->withDescription("Maximum Kafka protocol request message size")->isRequired(false)->build());

core::Property PublishKafka::RequestTimeOut(
    core::PropertyBuilder::createProperty("Request Timeout")->withDescription("The ack timeout of the producer request")->isRequired(false)->withDefaultValue<core::TimePeriodValue>(
        "10 sec")->supportsExpressionLanguage(true)->build());

core::Property PublishKafka::MessageTimeOut(
    core::PropertyBuilder::createProperty("Message Timeout")->withDescription("The total time sending a message could take")->isRequired(false)->withDefaultValue<core::TimePeriodValue>(
        "30 sec")->supportsExpressionLanguage(true)->build());

core::Property PublishKafka::ClientName(
    core::PropertyBuilder::createProperty("Client Name")->withDescription("Client Name to use when communicating with Kafka")->isRequired(true)->supportsExpressionLanguage(true)->build());

/**
 * These don't appear to need EL support
 */

core::Property PublishKafka::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("Maximum number of messages batched in one MessageSet")->isRequired(false)->withDefaultValue<uint32_t>(10)->build());
core::Property PublishKafka::TargetBatchPayloadSize(
    core::PropertyBuilder::createProperty("Target Batch Payload Size")->withDescription("The target total payload size for a batch. 0 B means unlimited (Batch Size is still applied).")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("512 KB")->build());
core::Property PublishKafka::AttributeNameRegex("Attributes to Send as Headers", "Any attribute whose name matches the regex will be added to the Kafka messages as a Header", "");
core::Property PublishKafka::QueueBufferMaxTime("Queue Buffering Max Time", "Delay to wait for messages in the producer queue to accumulate before constructing message batches", "");
core::Property PublishKafka::QueueBufferMaxSize("Queue Max Buffer Size", "Maximum total message size sum allowed on the producer queue", "");
core::Property PublishKafka::QueueBufferMaxMessage("Queue Max Message", "Maximum number of messages allowed on the producer queue", "");
core::Property PublishKafka::CompressCodec("Compress Codec", "compression codec to use for compressing message sets", COMPRESSION_CODEC_NONE);
core::Property PublishKafka::MaxFlowSegSize(
    core::PropertyBuilder::createProperty("Max Flow Segment Size")->withDescription("Maximum flow content payload segment size for the kafka record. 0 B means unlimited.")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("0 B")->build());
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
core::Property PublishKafka::DebugContexts("Debug contexts", "A comma-separated list of debug contexts to enable."
                                           "Including: generic, broker, topic, metadata, feature, queue, msg, protocol, cgrp, security, fetch, interceptor, plugin, consumer, admin, eos, all", "");

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
  properties.insert(MessageTimeOut);
  properties.insert(ClientName);
  properties.insert(AttributeNameRegex);
  properties.insert(BatchSize);
  properties.insert(TargetBatchPayloadSize);
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
  properties.insert(DebugContexts);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void PublishKafka::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  interrupted_ = false;
}

void PublishKafka::notifyStop() {
  logger_->log_debug("notifyStop called");
  interrupted_ = true;
  std::lock_guard<std::mutex> lock(messages_mutex_);
  for (auto& messages : messages_set_) {
    messages->interrupt();
  }
}

/**
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */
void PublishKafka::messageDeliveryCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* /*opaque*/) {
  if (rkmessage->_private == nullptr) {
    return;
  }
  std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>* func =
    reinterpret_cast<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>*>(rkmessage->_private);
  try {
    (*func)(rk, rkmessage);
  } catch (...) {
  }
  delete func;
}

bool PublishKafka::configureNewConnection(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context) {
  std::string value;
  int64_t valInt;
  std::string valueConf;
  std::array<char, 512U> errstr;
  rd_kafka_conf_res_t result;

  rd_kafka_conf_t* conf_ = rd_kafka_conf_new();
  if (conf_ == nullptr) {
    logger_->log_error("Failed to create rd_kafka_conf_t object");
    return false;
  }
  utils::ScopeGuard confGuard([conf_](){
    rd_kafka_conf_destroy(conf_);
  });

  auto key = conn->getKey();

  if (key->brokers_.empty()) {
    logger_->log_error("There are no brokers");
    return false;
  }
  result = rd_kafka_conf_set(conf_, "bootstrap.servers", key->brokers_.c_str(), errstr.data(), errstr.size());
  logger_->log_debug("PublishKafka: bootstrap.servers [%s]", key->brokers_);
  if (result != RD_KAFKA_CONF_OK) {
    logger_->log_error("PublishKafka: configure error result [%s]", errstr);
    return false;
  }

  if (key->client_id_.empty()) {
    logger_->log_error("Client id is empty");
    return false;
  }
  result = rd_kafka_conf_set(conf_, "client.id", key->client_id_.c_str(), errstr.data(), errstr.size());
  logger_->log_debug("PublishKafka: client.id [%s]", key->client_id_);
  if (result != RD_KAFKA_CONF_OK) {
    logger_->log_error("PublishKafka: configure error result [%s]", errstr);
    return false;
  }

  value = "";
  if (context->getProperty(DebugContexts.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "debug", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: debug [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure debug error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(KerberosServiceName.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.service.name", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.service.name [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(KerberosPrincipal.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.principal", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.principal [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(KerberosKeytabPath.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "sasl.kerberos.keytab", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.keytab [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }

  value = "";
  if (context->getProperty(MaxMessageSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    valueConf = std::to_string(valInt);
    result = rd_kafka_conf_set(conf_, "message.max.bytes", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: message.max.bytes [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxMessage.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "queue.buffering.max.messages", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: queue.buffering.max.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    valInt = valInt / 1024;
    valueConf = std::to_string(valInt);
    result = rd_kafka_conf_set(conf_, "queue.buffering.max.kbytes", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: queue.buffering.max.kbytes [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxTime.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      result = rd_kafka_conf_set(conf_, "queue.buffering.max.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: queue.buffering.max.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure queue buffer error result [%s]", errstr);
        return false;
      }
    }
  }
  value = "";
  if (context->getProperty(BatchSize.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_, "batch.num.messages", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: batch.num.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure batch size error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(CompressCodec.getName(), value) && !value.empty() && value != "none") {
    result = rd_kafka_conf_set(conf_, "compression.codec", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: compression.codec [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure compression codec error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(SecurityProtocol.getName(), value) && !value.empty()) {
    if (value == SECURITY_PROTOCOL_SSL) {
      result = rd_kafka_conf_set(conf_, "security.protocol", value.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: security.protocol [%s]", value);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        return false;
      }
      value = "";
      if (context->getProperty(SecurityCA.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_, "ssl.ca.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.ca.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          logger_->log_error("PublishKafka: configure error result [%s]", errstr);
          return false;
        }
      }
      value = "";
      if (context->getProperty(SecurityCert.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_, "ssl.certificate.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.certificate.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          logger_->log_error("PublishKafka: configure error result [%s]", errstr);
          return false;
        }
      }
      value = "";
      if (context->getProperty(SecurityPrivateKey.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_, "ssl.key.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.key.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          logger_->log_error("PublishKafka: configure error result [%s]", errstr);
          return false;
        }
      }
      value = "";
      if (context->getProperty(SecurityPrivateKeyPassWord.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_, "ssl.key.password", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.key.password [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          logger_->log_error("PublishKafka: configure error result [%s]", errstr);
          return false;
        }
      }
    } else {
      logger_->log_error("PublishKafka: unknown Security Protocol: %s", value);
      return false;
    }
  }

  // Add all of the dynamic properties as librdkafka configurations
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("PublishKafka registering %d librdkafka dynamic properties", dynamic_prop_keys.size());

  for (const auto &key : dynamic_prop_keys) {
    value = "";
    if (context->getDynamicProperty(key, value) && !value.empty()) {
      logger_->log_debug("PublishKafka: DynamicProperty: [%s] -> [%s]", key, value);
      result = rd_kafka_conf_set(conf_, key.c_str(), value.c_str(), errstr.data(), errstr.size());
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure error result [%s]", errstr);
        return false;
      }
    } else {
      logger_->log_warn("PublishKafka Dynamic Property '%s' is empty and therefore will not be configured", key);
    }
  }

  // Set the delivery callback
  rd_kafka_conf_set_dr_msg_cb(conf_, &PublishKafka::messageDeliveryCallback);

  // Set the logger callback
  rd_kafka_conf_set_log_cb(conf_, &KafkaConnection::logCallback);

  rd_kafka_t* producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf_, errstr.data(), errstr.size());

  if (producer == nullptr) {
    logger_->log_error("Failed to create Kafka producer %s", errstr);
    return false;
  }

  // The producer took ownership of the configuration, we must not free it
  confGuard.disable();

  conn->setConnection(producer);

  return true;
}

bool PublishKafka::createNewTopic(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name) {
  rd_kafka_topic_conf_t* topic_conf_ = rd_kafka_topic_conf_new();
  if (topic_conf_ == nullptr) {
    logger_->log_error("Failed to create rd_kafka_topic_conf_t object");
    return false;
  }
  utils::ScopeGuard confGuard([topic_conf_](){
    rd_kafka_topic_conf_destroy(topic_conf_);
  });

  rd_kafka_conf_res_t result;
  std::string value;
  std::array<char, 512U> errstr;
  int64_t valInt;
  std::string valueConf;

  value = "";
  if (context->getProperty(DeliveryGuarantee.getName(), value) && !value.empty()) {
    rd_kafka_topic_conf_set(topic_conf_, "request.required.acks", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: request.required.acks [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure request.required.acks error result [%s]", errstr);
      return false;
    }
  }
  value = "";
  if (context->getProperty(RequestTimeOut.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) &&
        core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      rd_kafka_topic_conf_set(topic_conf_, "request.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: request.timeout.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure request.timeout.ms error result [%s]", errstr);
        return false;
      }
    }
  }
  value = "";
  if (context->getProperty(MessageTimeOut.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) &&
        core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      rd_kafka_topic_conf_set(topic_conf_, "message.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: message.timeout.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure message.timeout.ms error result [%s]", errstr);
        return false;
      }
    }
  }

  rd_kafka_topic_t* topic_reference = rd_kafka_topic_new(conn->getConnection(), topic_name.c_str(), topic_conf_);
  if (topic_reference == nullptr) {
    rd_kafka_resp_err_t resp_err = rd_kafka_last_error();
    logger_->log_error("PublishKafka: failed to create topic %s, error: %s", topic_name.c_str(), rd_kafka_err2str(resp_err));
    return false;
  }

  // The topic took ownership of the configuration, we must not free it
  confGuard.disable();

  std::shared_ptr<KafkaTopic> kafkaTopicref = std::make_shared<KafkaTopic>(topic_reference);

  conn->putTopic(topic_name, kafkaTopicref);

  return true;
}

void PublishKafka::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  // Check whether we have been interrupted
  if (interrupted_) {
    logger_->log_info("The processor has been interrupted, not running onTrigger");
    context->yield();
    return;
  }

  // Try to get a KafkaConnection
  std::string client_id, brokers;
  if (!context->getProperty(ClientName.getName(), client_id)) {
    logger_->log_error("Client Name property missing or invalid");
    context->yield();
    return;
  }
  if (!context->getProperty(SeedBrokers.getName(), brokers)) {
    logger_->log_error("Knowb Brokers property missing or invalid");
    context->yield();
    return;
  }

  KafkaConnectionKey key;
  key.brokers_ = brokers;
  key.client_id_ = client_id;

  std::unique_ptr<KafkaLease> lease = connection_pool_.getOrCreateConnection(key);
  if (lease == nullptr) {
    logger_->log_info("This connection is used by another thread.");
    context->yield();
    return;
  }

  std::shared_ptr<KafkaConnection> conn = lease->getConn();
  if (!conn->initialized()) {
    logger_->log_trace("Connection not initialized to %s, %s", client_id, brokers);
    if (!configureNewConnection(conn, context)) {
      logger_->log_error("Could not configure Kafka Connection");
      context->yield();
      return;
    }
  }

  // Get some properties not (only) used directly to set up librdkafka
  std::string value;

  // Batch Size
  uint32_t batch_size;
  value = "";
  if (context->getProperty(BatchSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, batch_size)) {
    logger_->log_debug("PublishKafka: Batch Size [%lu]", batch_size);
  } else {
    batch_size = 10;
  }

  // Target Batch Payload Size
  uint64_t target_batch_payload_size;
  value = "";
  if (context->getProperty(TargetBatchPayloadSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, target_batch_payload_size)) {
    logger_->log_debug("PublishKafka: Target Batch Payload Size [%llu]", target_batch_payload_size);
  } else {
    target_batch_payload_size = 512 * 1024U;
  }

  // Max Flow Segment Size
  uint64_t max_flow_seg_size;
  value = "";
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, max_flow_seg_size)) {
    logger_->log_debug("PublishKafka: Max Flow Segment Size [%llu]", max_flow_seg_size);
  } else {
    max_flow_seg_size = 0U;
  }

  // Attributes to Send as Headers
  utils::Regex attributeNameRegex;
  value = "";
  if (context->getProperty(AttributeNameRegex.getName(), value) && !value.empty()) {
    attributeNameRegex = utils::Regex(value);
    logger_->log_debug("PublishKafka: AttributeNameRegex [%s]", value);
  }

  // Collect FlowFiles to process
  uint64_t actual_bytes = 0U;
  std::vector<std::shared_ptr<core::FlowFile>> flowFiles;
  for (uint32_t i = 0; i < batch_size; i++) {
    std::shared_ptr<core::FlowFile> flowFile = session->get();
    if (flowFile == nullptr) {
      break;
    }
    actual_bytes += flowFile->getSize();
    flowFiles.emplace_back(std::move(flowFile));
    if (target_batch_payload_size != 0U && actual_bytes >= target_batch_payload_size) {
      break;
    }
  }
  if (flowFiles.empty()) {
    return;
  }
  logger_->log_debug("Processing %lu flow files with a total size of %llu B", flowFiles.size(), actual_bytes);

  auto messages = std::make_shared<Messages>();
  // We must add this to the messages set, so that it will be interrupted when notifyStop is called
  {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    messages_set_.emplace(messages);
  }
  // We also have to insure that it will be removed once we are done with it
  utils::ScopeGuard messagesSetGuard([&]() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    messages_set_.erase(messages);
  });

  // Process FlowFiles
  for (auto& flowFile : flowFiles) {
    size_t flow_file_index = messages->addFlowFile();

    // Get Topic (FlowFile-dependent EL property)
    std::string topic;
    if (!context->getProperty(Topic, topic, flowFile)) {
      logger_->log_error("Flow file %s does not have a valid Topic", flowFile->getUUIDStr());
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
      continue;
    }

    // Add topic to the connection if needed
    if (!conn->hasTopic(topic)) {
      if (!createNewTopic(conn, context, topic)) {
        logger_->log_error("Failed to add topic %s", topic);
        messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
          flow_file_result.flow_file_error = true;
        });
        continue;
      }
    }

    std::string kafkaKey;
    kafkaKey = "";
    if (context->getDynamicProperty(MessageKeyField, kafkaKey, flowFile) && !kafkaKey.empty()) {
      logger_->log_debug("PublishKafka: Message Key Field [%s]", kafkaKey);
    } else {
      kafkaKey = flowFile->getUUIDStr();
    }

    auto thisTopic = conn->getTopic(topic);
    if (thisTopic == nullptr) {
      logger_->log_error("Topic %s is invalid", topic);
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
      continue;
    }

    PublishKafka::ReadCallback callback(max_flow_seg_size, kafkaKey, thisTopic->getTopic(), conn->getConnection(), flowFile,
                                        attributeNameRegex, messages, flow_file_index);
    session->read(flowFile, &callback);
    if (callback.status_ < 0) {
      logger_->log_error("Failed to send flow to kafka topic %s", topic);
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
      continue;
    }
  }

  logger_->log_trace("PublishKafka::onTrigger waitForCompletion start");
  messages->waitForCompletion();
  if (messages->wasInterrupted()) {
    logger_->log_warn("Waiting for delivery confirmation was interrupted, some flow files might be routed to Failure, even if they were successfully delivered.");
  }
  logger_->log_trace("PublishKafka::onTrigger waitForCompletion finish");

  messages->iterateFlowFiles([&](size_t index, const FlowFileResult& flow_file) {
    bool success;
    if (flow_file.flow_file_error) {
      success = false;
    } else if (flow_file.messages.empty()) {
      success = false;
      logger_->log_error("Assertion error: no messages found for flow file %s", flowFiles[index]->getUUIDStr());
    } else {
      success = true;
      for (size_t segment_num = 0; segment_num < flow_file.messages.size(); segment_num++) {
        const auto& message = flow_file.messages[segment_num];
        if (!message.completed) {
          success = false;
          logger_->log_error("Waiting for delivery confirmation was interrupted for flow file %s segment %zu", flowFiles[index]->getUUIDStr(), segment_num);
        } else if (message.is_error) {
          success = false;
          logger_->log_error("Failed to deliver flow file %s segment %zu, error: %s", flowFiles[index]->getUUIDStr(), segment_num,
                             rd_kafka_err2str(message.err_code));
        } else {
          logger_->log_debug("Successfully delivered flow file %s segment %zu", flowFiles[index]->getUUIDStr(), segment_num);
        }
      }
    }
    if (success) {
      session->transfer(flowFiles[index], Success);
    } else {
      session->transfer(flowFiles[index], Failure);
    }
  });
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

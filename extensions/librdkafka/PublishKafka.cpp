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

#include <cstdio>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <vector>

#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/ScopeGuard.h"
#include "utils/GeneralUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const core::Property PublishKafka::SeedBrokers(
    core::PropertyBuilder::createProperty("Known Brokers")->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::Topic(
    core::PropertyBuilder::createProperty("Topic Name")->withDescription("The Kafka Topic of interest")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::DeliveryGuarantee(
    core::PropertyBuilder::createProperty("Delivery Guarantee")->withDescription("Specifies the requirement for guaranteeing that a message is sent to Kafka. "
                                                                                 "Valid values are 0 (do not wait for acks), "
                                                                                 "-1 or all (block until message is committed by all in sync replicas) "
                                                                                 "or any concrete number of nodes.")
        ->isRequired(false)->supportsExpressionLanguage(true)->withDefaultValue(DELIVERY_ONE_NODE)->build());

const core::Property PublishKafka::MaxMessageSize(
    core::PropertyBuilder::createProperty("Max Request Size")->withDescription("Maximum Kafka protocol request message size")
        ->isRequired(false)->build());

const core::Property PublishKafka::RequestTimeOut(
    core::PropertyBuilder::createProperty("Request Timeout")->withDescription("The ack timeout of the producer request")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("10 sec")->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::MessageTimeOut(
    core::PropertyBuilder::createProperty("Message Timeout")->withDescription("The total time sending a message could take")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("30 sec")->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::ClientName(
    core::PropertyBuilder::createProperty("Client Name")->withDescription("Client Name to use when communicating with Kafka")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

/**
 * These don't appear to need EL support
 */

const core::Property PublishKafka::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("Maximum number of messages batched in one MessageSet")
        ->isRequired(false)->withDefaultValue<uint32_t>(10)->build());
const core::Property PublishKafka::TargetBatchPayloadSize(
    core::PropertyBuilder::createProperty("Target Batch Payload Size")->withDescription("The target total payload size for a batch. 0 B means unlimited (Batch Size is still applied).")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("512 KB")->build());
const core::Property PublishKafka::AttributeNameRegex("Attributes to Send as Headers", "Any attribute whose name matches the regex will be added to the Kafka messages as a Header", "");

const core::Property PublishKafka::QueueBufferMaxTime(
        core::PropertyBuilder::createProperty("Queue Buffering Max Time")
        ->isRequired(false)
        ->withDefaultValue<core::TimePeriodValue>("10 sec")
        ->withDescription("Delay to wait for messages in the producer queue to accumulate before constructing message batches")
        ->build());
const core::Property PublishKafka::QueueBufferMaxSize(
        core::PropertyBuilder::createProperty("Queue Max Buffer Size")
        ->isRequired(false)
        ->withDefaultValue<core::DataSizeValue>("1 MB")
        ->withDescription("Maximum total message size sum allowed on the producer queue")
        ->build());
const core::Property PublishKafka::QueueBufferMaxMessage(
        core::PropertyBuilder::createProperty("Queue Max Message")
        ->isRequired(false)
        ->withDefaultValue<uint64_t>(1000)
        ->withDescription("Maximum number of messages allowed on the producer queue")
        ->build());
const core::Property PublishKafka::CompressCodec(
        core::PropertyBuilder::createProperty("Compress Codec")
        ->isRequired(false)
        ->withDefaultValue<std::string>(COMPRESSION_CODEC_NONE)
        ->withAllowableValues<std::string>({COMPRESSION_CODEC_NONE, COMPRESSION_CODEC_GZIP, COMPRESSION_CODEC_SNAPPY})
        ->withDescription("compression codec to use for compressing message sets")
        ->build());

const core::Property PublishKafka::MaxFlowSegSize(
    core::PropertyBuilder::createProperty("Max Flow Segment Size")->withDescription("Maximum flow content payload segment size for the kafka record. 0 B means unlimited.")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("0 B")->build());
const core::Property PublishKafka::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
const core::Property PublishKafka::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
const core::Property PublishKafka::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKeyPassWord("Security Pass Phrase", "Private key passphrase", "");
const core::Property PublishKafka::KerberosServiceName("Kerberos Service Name", "Kerberos Service Name", "");
const core::Property PublishKafka::KerberosPrincipal("Kerberos Principal", "Keberos Principal", "");
const core::Property PublishKafka::KerberosKeytabPath("Kerberos Keytab Path",
                                                "The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.", "");
const core::Property PublishKafka::MessageKeyField("Message Key Field", "The name of a field in the Input Records that should be used as the Key for the Kafka message.\n"
                                             "Supports Expression Language: true (will be evaluated using flow file attributes)",
                                             "");
const core::Property PublishKafka::DebugContexts("Debug contexts", "A comma-separated list of debug contexts to enable."
                                           "Including: generic, broker, topic, metadata, feature, queue, msg, protocol, cgrp, security, fetch, interceptor, plugin, consumer, admin, eos, all", "");
const core::Property PublishKafka::FailEmptyFlowFiles(
    core::PropertyBuilder::createProperty("Fail empty flow files")
        ->withDescription("Keep backwards compatibility with <=0.7.0 bug which caused flow files with empty content to not be published to Kafka and forwarded to failure. The old behavior is "
                          "deprecated. Use connections to drop empty flow files!")
        ->isRequired(false)
        ->withDefaultValue<bool>(true)
        ->build());

const core::Relationship PublishKafka::Success("success", "Any FlowFile that is successfully sent to Kafka will be routed to this Relationship");
const core::Relationship PublishKafka::Failure("failure", "Any FlowFile that cannot be sent to Kafka will be routed to this Relationship");


namespace {
struct rd_kafka_conf_deleter {
  void operator()(rd_kafka_conf_t* p) const noexcept { rd_kafka_conf_destroy(p); }
};
struct rd_kafka_topic_conf_deleter {
  void operator()(rd_kafka_topic_conf_t* p) const noexcept { rd_kafka_topic_conf_destroy(p); }
};
}  // namespace

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
  properties.insert(FailEmptyFlowFiles);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Failure);
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void PublishKafka::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  interrupted_ = false;

  // Try to get a KafkaConnection
  std::string client_id, brokers;
  if (!context->getProperty(ClientName.getName(), client_id)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Client Name property missing or invalid");
  }
  if (!context->getProperty(SeedBrokers.getName(), brokers)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Known Brokers property missing or invalid");
  }

  // Get some properties not (only) used directly to set up librdkafka

  // Batch Size
  context->getProperty(BatchSize.getName(), batch_size_);
  logger_->log_debug("PublishKafka: Batch Size [%lu]", batch_size_);

  // Target Batch Payload Size
  context->getProperty(TargetBatchPayloadSize.getName(), target_batch_payload_size_);
  logger_->log_debug("PublishKafka: Target Batch Payload Size [%llu]", target_batch_payload_size_);

  // Max Flow Segment Size
  context->getProperty(MaxFlowSegSize.getName(), max_flow_seg_size_);
  logger_->log_debug("PublishKafka: Max Flow Segment Size [%llu]", max_flow_seg_size_);

  // Attributes to Send as Headers
  std::string value;
  if (context->getProperty(AttributeNameRegex.getName(), value) && !value.empty()) {
    attributeNameRegex_ = utils::Regex(value);
    logger_->log_debug("PublishKafka: AttributeNameRegex [%s]", value);
  }

  key_.brokers_ = brokers;
  key_.client_id_ = client_id;

  conn_ = utils::make_unique<KafkaConnection>(key_);
  configureNewConnection(context);

  logger_->log_debug("Successfully configured PublishKafka");
}

void PublishKafka::notifyStop() {
  logger_->log_debug("notifyStop called");
  interrupted_ = true;
  std::lock_guard<std::mutex> lock(messages_mutex_);
  for (auto& messages : messages_set_) {
    messages->interrupt();
  }
  conn_.reset();
}

/**
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */
void PublishKafka::messageDeliveryCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* /*opaque*/) {
  if (rkmessage->_private == nullptr) {
    return;
  }
  // allocated in PublishKafka::ReadCallback::produce
  auto* func = reinterpret_cast<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>*>(rkmessage->_private);
  try {
    (*func)(rk, rkmessage);
  } catch (...) { }
  delete func;
}

bool PublishKafka::configureNewConnection(const std::shared_ptr<core::ProcessContext> &context) {
  std::string value;
  int64_t valInt;
  std::string valueConf;
  std::array<char, 512U> errstr{};
  rd_kafka_conf_res_t result;
  const std::string PREFIX_ERROR_MSG = "PublishKafka: configure error result: ";

  std::unique_ptr<rd_kafka_conf_t, rd_kafka_conf_deleter> conf_{ rd_kafka_conf_new() };
  if (conf_ == nullptr) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create rd_kafka_conf_t object");
  }

  auto key = conn_->getKey();

  if (key->brokers_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "There are no brokers");
  }
  result = rd_kafka_conf_set(conf_.get(), "bootstrap.servers", key->brokers_.c_str(), errstr.data(), errstr.size());
  logger_->log_debug("PublishKafka: bootstrap.servers [%s]", key->brokers_);
  if (result != RD_KAFKA_CONF_OK) {
    auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  if (key->client_id_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Client id is empty");
  }
  result = rd_kafka_conf_set(conf_.get(), "client.id", key->client_id_.c_str(), errstr.data(), errstr.size());
  logger_->log_debug("PublishKafka: client.id [%s]", key->client_id_);
  if (result != RD_KAFKA_CONF_OK) {
    auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  value = "";
  if (context->getProperty(DebugContexts.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "debug", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: debug [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(KerberosServiceName.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "sasl.kerberos.service.name", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.service.name [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(KerberosPrincipal.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "sasl.kerberos.principal", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.principal [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(KerberosKeytabPath.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "sasl.kerberos.keytab", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: sasl.kerberos.keytab [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(MaxMessageSize.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "message.max.bytes", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: message.max.bytes [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxMessage.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "queue.buffering.max.messages", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: queue.buffering.max.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    valInt = valInt / 1024;
    valueConf = std::to_string(valInt);
    result = rd_kafka_conf_set(conf_.get(), "queue.buffering.max.kbytes", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: queue.buffering.max.kbytes [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(QueueBufferMaxTime.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      result = rd_kafka_conf_set(conf_.get(), "queue.buffering.max.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: queue.buffering.max.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
  }
  value = "";
  if (context->getProperty(BatchSize.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "batch.num.messages", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: batch.num.messages [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(CompressCodec.getName(), value) && !value.empty() && value != "none") {
    result = rd_kafka_conf_set(conf_.get(), "compression.codec", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: compression.codec [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  if (context->getProperty(SecurityProtocol.getName(), value) && !value.empty()) {
    if (value == SECURITY_PROTOCOL_SSL) {
      result = rd_kafka_conf_set(conf_.get(), "security.protocol", value.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: security.protocol [%s]", value);
      if (result != RD_KAFKA_CONF_OK) {
        auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
      value = "";
      if (context->getProperty(SecurityCA.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_.get(), "ssl.ca.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.ca.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
          throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
        }
      }
      value = "";
      if (context->getProperty(SecurityCert.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_.get(), "ssl.certificate.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.certificate.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
          throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
        }
      }
      value = "";
      if (context->getProperty(SecurityPrivateKey.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_.get(), "ssl.key.location", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.key.location [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
          throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
        }
      }
      value = "";
      if (context->getProperty(SecurityPrivateKeyPassWord.getName(), value) && !value.empty()) {
        result = rd_kafka_conf_set(conf_.get(), "ssl.key.password", value.c_str(), errstr.data(), errstr.size());
        logger_->log_debug("PublishKafka: ssl.key.password [%s]", value);
        if (result != RD_KAFKA_CONF_OK) {
          auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
          throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
        }
      }
    } else {
      auto error_msg = utils::StringUtils::join_pack("PublishKafka: unknown Security Protocol: ", value);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }

  // Add all of the dynamic properties as librdkafka configurations
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("PublishKafka registering %d librdkafka dynamic properties", dynamic_prop_keys.size());

  for (const auto &prop_key : dynamic_prop_keys) {
    value = "";
    if (context->getDynamicProperty(prop_key, value) && !value.empty()) {
      logger_->log_debug("PublishKafka: DynamicProperty: [%s] -> [%s]", prop_key, value);
      result = rd_kafka_conf_set(conf_.get(), prop_key.c_str(), value.c_str(), errstr.data(), errstr.size());
      if (result != RD_KAFKA_CONF_OK) {
        auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    } else {
      logger_->log_warn("PublishKafka Dynamic Property '%s' is empty and therefore will not be configured", prop_key);
    }
  }

  // Set the delivery callback
  rd_kafka_conf_set_dr_msg_cb(conf_.get(), &PublishKafka::messageDeliveryCallback);

  // Set the logger callback
  rd_kafka_conf_set_log_cb(conf_.get(), &KafkaConnection::logCallback);

  // The producer takes ownership of the configuration, we must not free it
  gsl::owner<rd_kafka_t*> producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf_.release(), errstr.data(), errstr.size());
  if (producer == nullptr) {
    auto error_msg = utils::StringUtils::join_pack("Failed to create Kafka producer ", errstr.data());
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  conn_->setConnection(producer);

  return true;
}

bool PublishKafka::createNewTopic(const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name) {
  std::unique_ptr<rd_kafka_topic_conf_t, rd_kafka_topic_conf_deleter> topic_conf_{ rd_kafka_topic_conf_new() };
  if (topic_conf_ == nullptr) {
    logger_->log_error("Failed to create rd_kafka_topic_conf_t object");
    return false;
  }

  rd_kafka_conf_res_t result;
  std::string value;
  std::array<char, 512U> errstr{};
  int64_t valInt;
  std::string valueConf;

  value = "";
  if (context->getProperty(DeliveryGuarantee.getName(), value) && !value.empty()) {
    /*
     * Because of a previous error in this processor, the default value of this property was "DELIVERY_ONE_NODE".
     * As this is not a valid value for "request.required.acks", the following rd_kafka_topic_conf_set call failed,
     * but because of an another error, this failure was silently ignored, meaning that the the default value for
     * "request.required.acks" did not change, and thus remained "-1". This means that having "DELIVERY_ONE_NODE" as
     * the value of this property actually caused the processor to wait for delivery ACKs from ALL nodes, instead
     * of just one. In order not to break configurations generated with earlier versions and keep the same behaviour
     * as they had, we have to map "DELIVERY_ONE_NODE" to "-1" here.
     */
    if (value == "DELIVERY_ONE_NODE") {
      value = "-1";
      logger_->log_warn("Using DELIVERY_ONE_NODE as the Delivery Guarantee property is deprecated and is translated to -1 "
                        "(block until message is committed by all in sync replicas) for backwards compatibility. "
                        "If you want to wait for one acknowledgment use '1' as the property.");
    }
    result = rd_kafka_topic_conf_set(topic_conf_.get(), "request.required.acks", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: request.required.acks [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure request.required.acks error result [%s]", errstr.data());
      return false;
    }
  }
  value = "";
  if (context->getProperty(RequestTimeOut.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) &&
        core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      valueConf = std::to_string(valInt);
      result = rd_kafka_topic_conf_set(topic_conf_.get(), "request.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: request.timeout.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure request.timeout.ms error result [%s]", errstr.data());
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
      result = rd_kafka_topic_conf_set(topic_conf_.get(), "message.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
      logger_->log_debug("PublishKafka: message.timeout.ms [%s]", valueConf);
      if (result != RD_KAFKA_CONF_OK) {
        logger_->log_error("PublishKafka: configure message.timeout.ms error result [%s]", errstr.data());
        return false;
      }
    }
  }

  // The topic takes ownership of the configuration, we must not free it
  gsl::owner<rd_kafka_topic_t*> topic_reference = rd_kafka_topic_new(conn_->getConnection(), topic_name.c_str(), topic_conf_.release());
  if (topic_reference == nullptr) {
    rd_kafka_resp_err_t resp_err = rd_kafka_last_error();
    logger_->log_error("PublishKafka: failed to create topic %s, error: %s", topic_name.c_str(), rd_kafka_err2str(resp_err));
    return false;
  }

  const auto kafkaTopicref = std::make_shared<KafkaTopic>(topic_reference);  // KafkaTopic takes ownership of topic_reference
  conn_->putTopic(topic_name, kafkaTopicref);

  return true;
}

void PublishKafka::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  // Check whether we have been interrupted
  if (interrupted_) {
    logger_->log_info("The processor has been interrupted, not running onTrigger");
    context->yield();
    return;
  }

  std::lock_guard<std::mutex> lock_connection(connection_mutex_);
  logger_->log_debug("PublishKafka onTrigger");

  // Collect FlowFiles to process
  uint64_t actual_bytes = 0U;
  std::vector<std::shared_ptr<core::FlowFile>> flowFiles;
  for (uint32_t i = 0; i < batch_size_; i++) {
    std::shared_ptr<core::FlowFile> flowFile = session->get();
    if (flowFile == nullptr) {
      break;
    }
    actual_bytes += flowFile->getSize();
    flowFiles.emplace_back(std::move(flowFile));
    if (target_batch_payload_size_ != 0U && actual_bytes >= target_batch_payload_size_) {
      break;
    }
  }
  if (flowFiles.empty()) {
    context->yield();
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
  const auto messagesSetGuard = gsl::finally([&]() {
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
    if (!conn_->hasTopic(topic)) {
      if (!createNewTopic(context, topic)) {
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

    auto thisTopic = conn_->getTopic(topic);
    if (thisTopic == nullptr) {
      logger_->log_error("Topic %s is invalid", topic);
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
      continue;
    }

    bool failEmptyFlowFiles = true;
    context->getProperty(FailEmptyFlowFiles.getName(), failEmptyFlowFiles);

    PublishKafka::ReadCallback callback(max_flow_seg_size_, kafkaKey, thisTopic->getTopic(), conn_->getConnection(), *flowFile,
                                        attributeNameRegex_, messages, flow_file_index, failEmptyFlowFiles);
    session->read(flowFile, &callback);

    if (!callback.called_) {
      // workaround: call callback since ProcessSession doesn't do so for empty flow files without resource claims
      callback.process(nullptr);
    }

    if (flowFile->getSize() == 0 && failEmptyFlowFiles) {
      logger_->log_debug("Deprecated behavior, use connections to drop empty flow files! Failing empty flow file with uuid: %s", flowFile->getUUIDStr());
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
    }

    if (callback.status_ < 0) {
      logger_->log_error("Failed to send flow to kafka topic %s, error: %s", topic, callback.error_);
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
    } else {
      success = true;
      for (size_t segment_num = 0; segment_num < flow_file.messages.size(); segment_num++) {
        const auto& message = flow_file.messages[segment_num];
        switch (message.status) {
          case MessageStatus::MESSAGESTATUS_UNCOMPLETE:
            success = false;
            logger_->log_error("Waiting for delivery confirmation was interrupted for flow file %s segment %zu",
                flowFiles[index]->getUUIDStr(),
                segment_num);
          break;
          case MessageStatus::MESSAGESTATUS_ERROR:
            success = false;
            logger_->log_error("Failed to deliver flow file %s segment %zu, error: %s",
                flowFiles[index]->getUUIDStr(),
                segment_num,
                rd_kafka_err2str(message.err_code));
          break;
          case MessageStatus::MESSAGESTATUS_SUCCESS:
            logger_->log_debug("Successfully delivered flow file %s segment %zu",
                flowFiles[index]->getUUIDStr(),
                segment_num);
          break;
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

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

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
#include <type_traits>
#include <vector>

#include "utils/gsl.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

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
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("10 sec")->build());

const core::Property PublishKafka::MessageTimeOut(
    core::PropertyBuilder::createProperty("Message Timeout")->withDescription("The total time sending a message could take")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

const core::Property PublishKafka::ClientName(
    core::PropertyBuilder::createProperty("Client Name")->withDescription("Client Name to use when communicating with Kafka")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

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
        ->withDefaultValue<core::TimePeriodValue>("5 millis")
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

const core::Property PublishKafka::SecurityCA("Security CA", "DEPRECATED in favor of SSL Context Service. File or directory path to CA certificate(s) for verifying the broker's key", "");
const core::Property PublishKafka::SecurityCert("Security Cert", "DEPRECATED in favor of SSL Context Service.Path to client's public key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKey("Security Private Key", "DEPRECATED in favor of SSL Context Service.Path to client's private key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKeyPassWord("Security Pass Phrase", "DEPRECATED in favor of SSL Context Service.Private key passphrase", "");
const core::Property PublishKafka::KafkaKey(
    core::PropertyBuilder::createProperty("Kafka Key")
        ->withDescription("The key to use for the message. If not specified, the UUID of the flow file is used as the message key.")
        ->supportsExpressionLanguage(true)
        ->build());
const core::Property PublishKafka::MessageKeyField("Message Key Field", "DEPRECATED, does not work -- use Kafka Key instead", "");

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

// Message
enum class MessageStatus : uint8_t {
  InFlight,
  Error,
  Success
};

const char* to_string(const MessageStatus s) {
  switch (s) {
    case MessageStatus::InFlight: return "InFlight";
    case MessageStatus::Error: return "Error";
    case MessageStatus::Success: return "Success";
  }
  throw std::runtime_error{"PublishKafka to_string(MessageStatus): unreachable code"};
}

struct MessageResult {
  MessageStatus status = MessageStatus::InFlight;
  rd_kafka_resp_err_t err_code = RD_KAFKA_RESP_ERR_NO_ERROR;
};

struct FlowFileResult {
  bool flow_file_error = false;
  std::vector<MessageResult> messages;
};
}  // namespace

class PublishKafka::Messages {
  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<FlowFileResult> flow_files_;
  bool interrupted_ = false;
  const std::shared_ptr<core::logging::Logger> logger_;

  std::string logStatus(const std::unique_lock<std::mutex>& lock) const {
    gsl_Expects(lock.owns_lock());
    const auto messageresult_ok = [](const MessageResult r) { return r.status == MessageStatus::Success && r.err_code == RD_KAFKA_RESP_ERR_NO_ERROR; };
    const auto messageresult_inflight = [](const MessageResult r) { return r.status == MessageStatus::InFlight && r.err_code == RD_KAFKA_RESP_ERR_NO_ERROR; };
    std::vector<size_t> flow_files_in_flight;
    std::ostringstream oss;
    if (interrupted_) { oss << "interrupted, "; }
    for (size_t ffi = 0; ffi < flow_files_.size(); ++ffi) {
      const auto& flow_file = flow_files_[ffi];
      if (!flow_file.flow_file_error && std::all_of(std::begin(flow_file.messages), std::end(flow_file.messages), messageresult_ok)) {
        continue;  // don't log the happy path to reduce log spam
      }
      if (!flow_file.flow_file_error && std::all_of(std::begin(flow_file.messages), std::end(flow_file.messages), messageresult_inflight)) {
        flow_files_in_flight.push_back(ffi);
        continue;  // don't log fully in-flight flow files here, log them at the end instead
      }
      oss << '[' << ffi << "]: {";
      if (flow_file.flow_file_error) { oss << "error, "; }
      for (size_t msgi = 0; msgi < flow_file.messages.size(); ++msgi) {
        const auto& msg = flow_file.messages[msgi];
        if (messageresult_ok(msg)) {
          continue;
        }
        oss << '<' << msgi << ">: (msg " << to_string(msg.status) << ", " << rd_kafka_err2str(msg.err_code) << "), ";
      }
      oss << "}, ";
    }
    oss << "in-flight (" << flow_files_in_flight.size() << "): " << utils::StringUtils::join(",", flow_files_in_flight);
    return oss.str();
  }

 public:
  explicit Messages(std::shared_ptr<core::logging::Logger> logger) :logger_{std::move(logger)} {}

  void waitForCompletion() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this, &lock] {
      if (logger_->should_log(core::logging::LOG_LEVEL::trace)) {
        logger_->log_trace("%s", logStatus(lock));
      }
      return interrupted_ || std::all_of(std::begin(this->flow_files_), std::end(this->flow_files_), [](const FlowFileResult& flow_file) {
        return flow_file.flow_file_error || std::all_of(std::begin(flow_file.messages), std::end(flow_file.messages), [](const MessageResult& message) {
          return message.status != MessageStatus::InFlight;
        });
      });
    });
  }

  template<typename Func>
  auto modifyResult(size_t index, Func fun) -> decltype(fun(flow_files_.at(index))) {
    std::unique_lock<std::mutex> lock(mutex_);
    const auto notifier = gsl::finally([this]{ cv_.notify_all(); });
    try {
      return fun(flow_files_.at(index));
    } catch(const std::exception& ex) {
      logger_->log_warn("Messages::modifyResult exception: %s", ex.what());
      throw;
    }
  }

  size_t addFlowFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    flow_files_.emplace_back();
    return flow_files_.size() - 1;
  }

  template<typename Func>
  auto iterateFlowFiles(Func fun) -> std::void_t<decltype(fun(size_t{0}, flow_files_.front()))> {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t index = 0U; index < flow_files_.size(); index++) {
      fun(index, flow_files_[index]);
    }
  }

  void interrupt() {
    std::unique_lock<std::mutex> lock(mutex_);
    interrupted_ = true;
    cv_.notify_all();
    gsl_Ensures(interrupted_);
  }

  bool wasInterrupted() {
    std::lock_guard<std::mutex> lock(mutex_);
    return interrupted_;
  }
};

namespace {
class ReadCallback {
 public:
  struct rd_kafka_headers_deleter {
    void operator()(rd_kafka_headers_t* ptr) const noexcept {
      rd_kafka_headers_destroy(ptr);
    }
  };

  using rd_kafka_headers_unique_ptr = std::unique_ptr<rd_kafka_headers_t, rd_kafka_headers_deleter>;

 private:
  void allocate_message_object(const size_t segment_num) const {
    messages_->modifyResult(flow_file_index_, [segment_num](FlowFileResult& flow_file) {
      // allocate message object to be filled in by the callback in produce()
      if (flow_file.messages.size() < segment_num + 1) {
        flow_file.messages.resize(segment_num + 1);
      }
      gsl_Ensures(flow_file.messages.size() > segment_num);
    });
  }

  static rd_kafka_headers_unique_ptr make_headers(const core::FlowFile& flow_file, utils::Regex& attribute_name_regex) {
    const gsl::owner<rd_kafka_headers_t*> result{ rd_kafka_headers_new(8) };
    if (!result) { throw std::bad_alloc{}; }

    for (const auto& kv : flow_file.getAttributes()) {
      if (utils::regexSearch(kv.first, attribute_name_regex)) {
        rd_kafka_header_add(result, kv.first.c_str(), kv.first.size(), kv.second.c_str(), kv.second.size());
      }
    }
    return rd_kafka_headers_unique_ptr{ result };
  }

  rd_kafka_resp_err_t produce(const size_t segment_num, std::vector<std::byte>& buffer, const size_t buflen) const {
    const std::shared_ptr<PublishKafka::Messages> messages_ptr_copy = this->messages_;
    const auto flow_file_index_copy = this->flow_file_index_;
    const auto logger = logger_;
    const auto produce_callback = [messages_ptr_copy, flow_file_index_copy, segment_num, logger](rd_kafka_t * /*rk*/, const rd_kafka_message_t *rkmessage) {
      messages_ptr_copy->modifyResult(flow_file_index_copy, [segment_num, rkmessage, logger, flow_file_index_copy](FlowFileResult &flow_file) {
        auto &message = flow_file.messages.at(segment_num);
        message.err_code = rkmessage->err;
        message.status = message.err_code == 0 ? MessageStatus::Success : MessageStatus::Error;
        if (message.err_code != RD_KAFKA_RESP_ERR_NO_ERROR) {
          logger->log_warn("delivery callback, flow file #%zu/segment #%zu: %s", flow_file_index_copy, segment_num, rd_kafka_err2str(message.err_code));
        } else {
          logger->log_debug("delivery callback, flow file #%zu/segment #%zu: success", flow_file_index_copy, segment_num);
        }
      });
    };
    // release()d below, deallocated in PublishKafka::messageDeliveryCallback
    auto callback_ptr = std::make_unique<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>>(std::move(produce_callback));

    allocate_message_object(segment_num);

    const gsl::owner<rd_kafka_headers_t*> hdrs_copy = rd_kafka_headers_copy(hdrs.get());
    const auto err = rd_kafka_producev(rk_, RD_KAFKA_V_RKT(rkt_), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(buffer.data(), buflen),
        RD_KAFKA_V_HEADERS(hdrs_copy), RD_KAFKA_V_KEY(key_.c_str(), key_.size()), RD_KAFKA_V_OPAQUE(callback_ptr.get()), RD_KAFKA_V_END);
    if (err == RD_KAFKA_RESP_ERR_NO_ERROR) {
      // in case of failure, messageDeliveryCallback is not called and callback_ptr will delete the callback
      // in case of success, messageDeliveryCallback takes ownership of the callback, so we no longer need to delete it
      (void)callback_ptr.release();
    } else {
      // in case of failure, rd_kafka_producev doesn't take ownership of the headers, so we need to delete them
      rd_kafka_headers_destroy(hdrs_copy);
    }
    logger_->log_trace("produce enqueued flow file #%zu/segment #%zu: %s", flow_file_index_, segment_num, rd_kafka_err2str(err));
    return err;
  }

 public:
  ReadCallback(const uint64_t max_seg_size,
      std::string key,
      rd_kafka_topic_t* const rkt,
      rd_kafka_t* const rk,
      const core::FlowFile& flowFile,
      utils::Regex& attributeNameRegex,
      std::shared_ptr<PublishKafka::Messages> messages,
      const size_t flow_file_index,
      const bool fail_empty_flow_files,
      std::shared_ptr<core::logging::Logger> logger)
      : flow_size_(flowFile.getSize()),
      max_seg_size_(max_seg_size == 0 || flow_size_ < max_seg_size ? flow_size_ : max_seg_size),
      key_(std::move(key)),
      rkt_(rkt),
      rk_(rk),
      hdrs(make_headers(flowFile, attributeNameRegex)),
      messages_(std::move(messages)),
      flow_file_index_(flow_file_index),
      fail_empty_flow_files_(fail_empty_flow_files),
      logger_(std::move(logger))
  { }

  ReadCallback(const ReadCallback&) = delete;
  ReadCallback& operator=(ReadCallback) = delete;

  int64_t operator()(const std::shared_ptr<io::BaseStream>& stream) {
    std::vector<std::byte> buffer;

    buffer.resize(max_seg_size_);
    read_size_ = 0;
    status_ = 0;
    called_ = true;

    gsl_Expects(max_seg_size_ != 0 || (flow_size_ == 0 && "max_seg_size_ == 0 implies flow_size_ == 0"));
    // ^^ therefore checking max_seg_size_ == 0 handles both division by zero and flow_size_ == 0 cases
    const size_t reserved_msg_capacity = max_seg_size_ == 0 ? 1 : utils::intdiv_ceil(flow_size_, max_seg_size_);
    messages_->modifyResult(flow_file_index_, [reserved_msg_capacity](FlowFileResult& flow_file) {
      flow_file.messages.reserve(reserved_msg_capacity);
    });

    // If the flow file is empty, we still want to send the message, unless the user wants to fail_empty_flow_files_
    if (flow_size_ == 0 && !fail_empty_flow_files_) {
      const auto err = produce(0, buffer, 0);
      if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        status_ = -1;
        error_ = rd_kafka_err2str(err);
      }
      return 0;
    }

    for (size_t segment_num = 0; read_size_ < flow_size_; ++segment_num) {
      const auto readRet = stream->read(buffer);
      if (io::isError(readRet)) {
        status_ = -1;
        error_ = "Failed to read from stream";
        return read_size_;
      }
      if (readRet == 0) { break; }

      const auto err = produce(segment_num, buffer, readRet);
      if (err) {
        messages_->modifyResult(flow_file_index_, [segment_num, err](FlowFileResult& flow_file) {
          auto& message = flow_file.messages.at(segment_num);
          message.status = MessageStatus::Error;
          message.err_code = err;
        });
        status_ = -1;
        error_ = rd_kafka_err2str(err);
        return read_size_;
      }
      read_size_ += readRet;
    }
    return read_size_;
  }

  const uint64_t flow_size_ = 0;
  const uint64_t max_seg_size_ = 0;
  const std::string key_;
  rd_kafka_topic_t* const rkt_ = nullptr;
  rd_kafka_t* const rk_ = nullptr;
  const rd_kafka_headers_unique_ptr hdrs;  // not null
  const std::shared_ptr<PublishKafka::Messages> messages_;
  const size_t flow_file_index_;
  int status_ = 0;
  std::string error_;
  uint32_t read_size_ = 0;
  bool called_ = false;
  const bool fail_empty_flow_files_ = true;
  const std::shared_ptr<core::logging::Logger> logger_;
};

/**
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */
void messageDeliveryCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* /*opaque*/) {
  if (rkmessage->_private == nullptr) {
    return;
  }
  // allocated in ReadCallback::produce
  auto* const func = reinterpret_cast<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>*>(rkmessage->_private);
  try {
    (*func)(rk, rkmessage);
  } catch (...) { }
  delete func;
}
}  // namespace

void PublishKafka::initialize() {
  // Set the supported properties
  setSupportedProperties({
    SeedBrokers,
    Topic,
    DeliveryGuarantee,
    MaxMessageSize,
    RequestTimeOut,
    MessageTimeOut,
    ClientName,
    AttributeNameRegex,
    BatchSize,
    TargetBatchPayloadSize,
    QueueBufferMaxTime,
    QueueBufferMaxSize,
    QueueBufferMaxMessage,
    CompressCodec,
    MaxFlowSegSize,
    SecurityProtocol,
    SSLContextService,
    SecurityCA,
    SecurityCert,
    SecurityPrivateKey,
    SecurityPrivateKeyPassWord,
    KerberosServiceName,
    KerberosPrincipal,
    KerberosKeytabPath,
    KafkaKey,
    MessageKeyField,
    DebugContexts,
    FailEmptyFlowFiles,
    SASLMechanism,
    Username,
    Password
  });
  // Set the supported relationships
  setSupportedRelationships({
    Success,
    Failure
  });
}

void PublishKafka::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  interrupted_ = false;

  // Try to get a KafkaConnection
  std::string client_id, brokers;
  if (!context->getProperty(ClientName, client_id, nullptr)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Client Name property missing or invalid");
  }
  if (!context->getProperty(SeedBrokers, brokers, nullptr)) {
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

  conn_ = std::make_unique<KafkaConnection>(key_);
  configureNewConnection(context);

  std::string message_key_field;
  if (context->getProperty(MessageKeyField.getName(), message_key_field) && !message_key_field.empty()) {
    logger_->log_error("The %s property is set. This property is DEPRECATED and has no effect; please use Kafka Key instead.", MessageKeyField.getName());
  }

  logger_->log_debug("Successfully configured PublishKafka");
}

void PublishKafka::notifyStop() {
  logger_->log_debug("notifyStop called");
  interrupted_ = true;
  {
    // Normally when we need both connection_mutex_ and messages_mutex_, we need to take connection_mutex_ first to avoid a deadlock.
    // It's not possible to do that here, because we need to interrupt the messages while onTrigger is running and holding connection_mutex_.
    // For this reason, we take messages_mutex_ only, interrupt the messages, then release the lock to let a possibly running onTrigger take it and finish.
    // After onTrigger finishes, we can take connection_mutex_ and close the connection without needing to wait for message finishes/timeouts in onTrigger.
    // A possible new onTrigger between our critical sections won't produce more messages because we set interrupted_ = true above.
    std::lock_guard<std::mutex> lock(messages_mutex_);
    for (auto& messages : messages_set_) {
      messages->interrupt();
    }
  }
  std::lock_guard<std::mutex> conn_lock(connection_mutex_);
  conn_.reset();
}


bool PublishKafka::configureNewConnection(const std::shared_ptr<core::ProcessContext> &context) {
  std::string value;
  int64_t valInt;
  std::string valueConf;
  std::array<char, 512U> errstr{};
  rd_kafka_conf_res_t result;
  const char* const PREFIX_ERROR_MSG = "PublishKafka: configure error result: ";

  std::unique_ptr<rd_kafka_conf_t, rd_kafka_conf_deleter> conf_{ rd_kafka_conf_new() };
  if (conf_ == nullptr) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Failed to create rd_kafka_conf_t object");
  }

  const auto* const key = conn_->getKey();

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
  if (context->getProperty(MaxMessageSize.getName(), value) && !value.empty()) {
    result = rd_kafka_conf_set(conf_.get(), "message.max.bytes", value.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: message.max.bytes [%s]", value);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
  value = "";
  uint32_t int_val;
  if (context->getProperty(QueueBufferMaxMessage.getName(), int_val)) {
    if (int_val < batch_size_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid configuration: Batch Size cannot be larger than Queue Max Message");
    }

    value = std::to_string(int_val);
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

  if (auto queue_buffer_max_time = context->getProperty<core::TimePeriodValue>(QueueBufferMaxTime)) {
    valueConf = std::to_string(queue_buffer_max_time->getMilliseconds().count());
    result = rd_kafka_conf_set(conf_.get(), "queue.buffering.max.ms", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: queue.buffering.max.ms [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
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

  setKafkaAuthenticationParameters(*context, gsl::make_not_null(conf_.get()));

  // Add all of the dynamic properties as librdkafka configurations
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("PublishKafka registering %d librdkafka dynamic properties", dynamic_prop_keys.size());

  for (const auto &prop_key : dynamic_prop_keys) {
    core::Property dynamic_property_key{prop_key, "dynamic property"};
    dynamic_property_key.setSupportsExpressionLanguage(true);
    std::string dynamic_property_value;
    if (context->getDynamicProperty(dynamic_property_key, dynamic_property_value, nullptr) && !dynamic_property_value.empty()) {
      logger_->log_debug("PublishKafka: DynamicProperty: [%s] -> [%s]", prop_key, dynamic_property_value);
      result = rd_kafka_conf_set(conf_.get(), prop_key.c_str(), dynamic_property_value.c_str(), errstr.data(), errstr.size());
      if (result != RD_KAFKA_CONF_OK) {
        auto error_msg = utils::StringUtils::join_pack(PREFIX_ERROR_MSG, errstr.data());
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    } else {
      logger_->log_warn("PublishKafka Dynamic Property '%s' is empty and therefore will not be configured", prop_key);
    }
  }

  // Set the delivery callback
  rd_kafka_conf_set_dr_msg_cb(conf_.get(), &messageDeliveryCallback);

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

bool PublishKafka::createNewTopic(const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name, const std::shared_ptr<core::FlowFile>& flow_file) {
  std::unique_ptr<rd_kafka_topic_conf_t, rd_kafka_topic_conf_deleter> topic_conf_{ rd_kafka_topic_conf_new() };
  if (topic_conf_ == nullptr) {
    logger_->log_error("Failed to create rd_kafka_topic_conf_t object");
    return false;
  }

  rd_kafka_conf_res_t result;
  std::string value;
  std::array<char, 512U> errstr{};
  std::string valueConf;

  value = "";
  if (context->getProperty(DeliveryGuarantee, value, flow_file) && !value.empty()) {
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

  if (auto request_timeout = context->getProperty<core::TimePeriodValue>(RequestTimeOut)) {
    valueConf = std::to_string(request_timeout->getMilliseconds().count());
    result = rd_kafka_topic_conf_set(topic_conf_.get(), "request.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: request.timeout.ms [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure request.timeout.ms error result [%s]", errstr.data());
      return false;
    }
  }

  if (auto message_timeout = context->getProperty<core::TimePeriodValue>(MessageTimeOut)) {
    valueConf = std::to_string(message_timeout->getMilliseconds().count());
    result = rd_kafka_topic_conf_set(topic_conf_.get(), "message.timeout.ms", valueConf.c_str(), errstr.data(), errstr.size());
    logger_->log_debug("PublishKafka: message.timeout.ms [%s]", valueConf);
    if (result != RD_KAFKA_CONF_OK) {
      logger_->log_error("PublishKafka: configure message.timeout.ms error result [%s]", errstr.data());
      return false;
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

std::optional<utils::SSL_data> PublishKafka::getSslData(core::ProcessContext& context) const {
  if (auto result = KafkaProcessorBase::getSslData(context); result) {
    return result;
  }

  utils::SSL_data ssl_data;
  context.getProperty(SecurityCA.getName(), ssl_data.ca_loc);
  context.getProperty(SecurityCert.getName(), ssl_data.cert_loc);
  context.getProperty(SecurityPrivateKey.getName(), ssl_data.key_loc);
  context.getProperty(SecurityPrivateKeyPassWord.getName(), ssl_data.key_pw);
  return ssl_data;
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

  auto messages = std::make_shared<Messages>(logger_);
  // We must add this to the messages set, so that it will be interrupted when notifyStop is called
  {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    messages_set_.emplace(messages);
  }
  // We also have to ensure that it will be removed once we are done with it
  const auto messagesSetGuard = gsl::finally([&]() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    messages_set_.erase(messages);
  });

  // Process FlowFiles
  for (auto& flowFile : flowFiles) {
    size_t flow_file_index = messages->addFlowFile();

    // Get Topic (FlowFile-dependent EL property)
    std::string topic;
    if (context->getProperty(Topic, topic, flowFile)) {
      logger_->log_debug("PublishKafka: topic for flow file %s is '%s'", flowFile->getUUIDStr(), topic);
    } else {
      logger_->log_error("Flow file %s does not have a valid Topic", flowFile->getUUIDStr());
      messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
        flow_file_result.flow_file_error = true;
      });
      continue;
    }

    // Add topic to the connection if needed
    if (!conn_->hasTopic(topic)) {
      if (!createNewTopic(context, topic, flowFile)) {
        logger_->log_error("Failed to add topic %s", topic);
        messages->modifyResult(flow_file_index, [](FlowFileResult& flow_file_result) {
          flow_file_result.flow_file_error = true;
        });
        continue;
      }
    }

    std::string kafkaKey;
    if (!context->getProperty(KafkaKey, kafkaKey, flowFile) || kafkaKey.empty()) {
      kafkaKey = flowFile->getUUIDStr();
    }
    logger_->log_debug("PublishKafka: Message Key [%s]", kafkaKey);

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

    ReadCallback callback(max_flow_seg_size_, kafkaKey, thisTopic->getTopic(), conn_->getConnection(), *flowFile,
                                        attributeNameRegex_, messages, flow_file_index, failEmptyFlowFiles, logger_);
    session->read(flowFile, std::ref(callback));

    if (!callback.called_) {
      // workaround: call callback since ProcessSession doesn't do so for empty flow files without resource claims
      callback(nullptr);
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
          case MessageStatus::InFlight:
            success = false;
            logger_->log_error("Waiting for delivery confirmation was interrupted for flow file %s segment %zu",
                flowFiles[index]->getUUIDStr(),
                segment_num);
          break;
          case MessageStatus::Error:
            success = false;
            logger_->log_error("Failed to deliver flow file %s segment %zu, error: %s",
                flowFiles[index]->getUUIDStr(),
                segment_num,
                rd_kafka_err2str(message.err_code));
          break;
          case MessageStatus::Success:
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
      session->penalize(flowFiles[index]);
      session->transfer(flowFiles[index], Failure);
    }
  });
}

REGISTER_RESOURCE(PublishKafka, "This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. "
                  "This message is optionally assigned a key by using the <Kafka Key> Property.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

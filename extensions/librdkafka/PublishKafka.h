/**
 * @file PublishKafka.h
 * PublishKafka class declaration
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
#ifndef EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_
#define EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <condition_variable>
#include <utility>
#include <vector>

#include "utils/GeneralUtils.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "utils/RegexUtils.h"
#include "rdkafka.h" // NOLINT
#include "KafkaConnection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define COMPRESSION_CODEC_NONE "none"
#define COMPRESSION_CODEC_GZIP "gzip"
#define COMPRESSION_CODEC_SNAPPY "snappy"
#define ROUND_ROBIN_PARTITIONING "Round Robin"
#define RANDOM_PARTITIONING "Random Robin"
#define USER_DEFINED_PARTITIONING "User-Defined"
#define DELIVERY_REPLICATED "all"
#define DELIVERY_ONE_NODE "1"
#define DELIVERY_BEST_EFFORT "0"
#define SECURITY_PROTOCOL_PLAINTEXT "plaintext"
#define SECURITY_PROTOCOL_SSL "ssl"
#define SECURITY_PROTOCOL_SASL_PLAINTEXT "sasl_plaintext"
#define SECURITY_PROTOCOL_SASL_SSL "sasl_ssl"
#define KAFKA_KEY_ATTRIBUTE "kafka.key"

// PublishKafka Class
class PublishKafka : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit PublishKafka(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(std::move(name), uuid),
        logger_(logging::LoggerFactory<PublishKafka>::getLogger()),
        interrupted_(false) {
  }

  virtual ~PublishKafka() = default;

  static constexpr char const* ProcessorName = "PublishKafka";

  // Supported Properties
  static const core::Property SeedBrokers;
  static const core::Property Topic;
  static const core::Property DeliveryGuarantee;
  static const core::Property MaxMessageSize;
  static const core::Property RequestTimeOut;
  static const core::Property MessageTimeOut;
  static const core::Property ClientName;
  static const core::Property BatchSize;
  static const core::Property TargetBatchPayloadSize;
  static const core::Property AttributeNameRegex;
  static const core::Property QueueBufferMaxTime;
  static const core::Property QueueBufferMaxSize;
  static const core::Property QueueBufferMaxMessage;
  static const core::Property CompressCodec;
  static const core::Property MaxFlowSegSize;
  static const core::Property SecurityProtocol;
  static const core::Property SecurityCA;
  static const core::Property SecurityCert;
  static const core::Property SecurityPrivateKey;
  static const core::Property SecurityPrivateKeyPassWord;
  static const core::Property KerberosServiceName;
  static const core::Property KerberosPrincipal;
  static const core::Property KerberosKeytabPath;
  static const core::Property MessageKeyField;
  static const core::Property DebugContexts;
  static const core::Property FailEmptyFlowFiles;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  // Message
  enum class MessageStatus : uint8_t {
    MESSAGESTATUS_UNCOMPLETE,
    MESSAGESTATUS_ERROR,
    MESSAGESTATUS_SUCCESS
  };

  struct MessageResult {
    MessageStatus status = MessageStatus::MESSAGESTATUS_UNCOMPLETE;
    rd_kafka_resp_err_t err_code = RD_KAFKA_RESP_ERR_UNKNOWN;
  };

  struct FlowFileResult {
    bool flow_file_error = false;
    std::vector<MessageResult> messages;
  };

  struct Messages {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<FlowFileResult> flow_files;
    bool interrupted = false;

    void waitForCompletion() {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [this]() -> bool {
        if (interrupted) {
          return true;
        }
        size_t index = 0U;
        return std::all_of(this->flow_files.begin(), this->flow_files.end(), [&](const FlowFileResult& flow_file) {
          index++;
          if (flow_file.flow_file_error) {
            return true;
          }
          return std::all_of(flow_file.messages.begin(), flow_file.messages.end(), [](const MessageResult& message) {
            return message.status != MessageStatus::MESSAGESTATUS_UNCOMPLETE;
          });
        });
      });
    }

    void modifyResult(size_t index, const std::function<void(FlowFileResult&)>& fun) {
      std::unique_lock<std::mutex> lock(mutex);
      fun(flow_files.at(index));
      cv.notify_all();
    }

    size_t addFlowFile() {
      std::lock_guard<std::mutex> lock(mutex);
      flow_files.emplace_back();
      return flow_files.size() - 1;
    }

    void iterateFlowFiles(const std::function<void(size_t /*index*/, const FlowFileResult& /*flow_file_result*/)>& fun) {
      std::lock_guard<std::mutex> lock(mutex);
      for (size_t index = 0U; index < flow_files.size(); index++) {
        fun(index, flow_files[index]);
      }
    }

    void interrupt() {
      std::unique_lock<std::mutex> lock(mutex);
      interrupted = true;
      cv.notify_all();
    }

    bool wasInterrupted() {
      std::lock_guard<std::mutex> lock(mutex);
      return interrupted;
    }
  };

  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
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
      });
    }

    static rd_kafka_headers_unique_ptr make_headers(const core::FlowFile& flow_file, utils::Regex& attribute_name_regex) {
      const gsl::owner<rd_kafka_headers_t*> result{ rd_kafka_headers_new(8) };
      if (!result) { throw std::bad_alloc{}; }

      for (const auto& kv : flow_file.getAttributes()) {
        if (attribute_name_regex.match(kv.first)) {
          rd_kafka_header_add(result, kv.first.c_str(), kv.first.size(), kv.second.c_str(), kv.second.size());
        }
      }
      return rd_kafka_headers_unique_ptr{ result };
    }

    rd_kafka_resp_err_t produce(const size_t segment_num, std::vector<unsigned char>& buffer, const size_t buflen) const {
      const std::shared_ptr<Messages> messages_ptr_copy = this->messages_;
      const auto flow_file_index_copy = this->flow_file_index_;
      const auto produce_callback = [messages_ptr_copy, flow_file_index_copy, segment_num](rd_kafka_t * /*rk*/, const rd_kafka_message_t *rkmessage) {
        messages_ptr_copy->modifyResult(flow_file_index_copy, [segment_num, rkmessage](FlowFileResult &flow_file) {
          auto &message = flow_file.messages.at(segment_num);
          message.err_code = rkmessage->err;
          message.status = message.err_code == 0 ? MessageStatus::MESSAGESTATUS_SUCCESS : MessageStatus::MESSAGESTATUS_ERROR;
        });
      };
      // release()d below, deallocated in PublishKafka::messageDeliveryCallback
      auto callback_ptr = utils::make_unique<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>>(std::move(produce_callback));

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
      return err;
    }

   public:
    ReadCallback(const uint64_t max_seg_size,
                 std::string key,
                 rd_kafka_topic_t * const rkt,
                 rd_kafka_t * const rk,
                 const core::FlowFile& flowFile,
                 utils::Regex &attributeNameRegex,
                 std::shared_ptr<Messages> messages,
                 const size_t flow_file_index,
                 const bool fail_empty_flow_files)
        : flow_size_(flowFile.getSize()),
          max_seg_size_(max_seg_size == 0 || flow_size_ < max_seg_size ? flow_size_ : max_seg_size),
          key_(std::move(key)),
          rkt_(rkt),
          rk_(rk),
          hdrs(make_headers(flowFile, attributeNameRegex)),
          messages_(std::move(messages)),
          flow_file_index_(flow_file_index),
          fail_empty_flow_files_(fail_empty_flow_files)
    { }

    int64_t process(const std::shared_ptr<io::BaseStream> stream) {
      std::vector<unsigned char> buffer;

      buffer.resize(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      called_ = true;

      assert(max_seg_size_ != 0 || flow_size_ == 0 && "max_seg_size_ == 0 implies flow_size_ == 0");
      // ^^ therefore checking max_seg_size_ == 0 handles both division by zero and flow_size_ == 0 cases
      const size_t reserved_msg_capacity = max_seg_size_ == 0 ? 1 : utils::intdiv_ceil(flow_size_, max_seg_size_);
      messages_->modifyResult(flow_file_index_, [reserved_msg_capacity](FlowFileResult& flow_file) {
        flow_file.messages.reserve(reserved_msg_capacity);
      });

      // If the flow file is empty, we still want to send the message, unless the user wants to fail_empty_flow_files_
      if (flow_size_ == 0 && !fail_empty_flow_files_) {
        produce(0, buffer, 0);
        return 0;
      }

      for (size_t segment_num = 0; read_size_ < flow_size_; ++segment_num) {
        const int readRet = stream->read(buffer.data(), buffer.size());
        if (readRet < 0) {
          status_ = -1;
          error_ = "Failed to read from stream";
          return read_size_;
        }

        if (readRet <= 0) { break; }

        const auto err = produce(segment_num, buffer, readRet);
        if (err) {
          messages_->modifyResult(flow_file_index_, [segment_num, err](FlowFileResult& flow_file) {
            auto& message = flow_file.messages.at(segment_num);
            message.status = MessageStatus::MESSAGESTATUS_ERROR;
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
    rd_kafka_topic_t * const rkt_ = nullptr;
    rd_kafka_t * const rk_ = nullptr;
    const rd_kafka_headers_unique_ptr hdrs;  // not null
    const std::shared_ptr<Messages> messages_;
    const size_t flow_file_index_;
    int status_ = 0;
    std::string error_;
    int read_size_ = 0;
    bool called_ = false;
    const bool fail_empty_flow_files_ = true;
  };

 public:
  bool supportsDynamicProperties() override {
    return true;
  }

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void notifyStop() override;

 protected:
  bool configureNewConnection(const std::shared_ptr<core::ProcessContext> &context);
  bool createNewTopic(const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name);

 private:
  static void messageDeliveryCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque);

  std::shared_ptr<logging::Logger> logger_;

  KafkaConnectionKey key_;
  std::unique_ptr<KafkaConnection> conn_;
  std::mutex connection_mutex_;

  uint32_t batch_size_;
  uint64_t target_batch_payload_size_;
  uint64_t max_flow_seg_size_;
  utils::Regex attributeNameRegex_;

  std::atomic<bool> interrupted_;
  std::mutex messages_mutex_;
  std::set<std::shared_ptr<Messages>> messages_set_;
};

REGISTER_RESOURCE(PublishKafka, "This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. "
                  "This message is optionally assigned a key by using the <Kafka Key> Property.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_

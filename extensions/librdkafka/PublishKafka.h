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
#ifndef __PUT_KAFKA_H__
#define __PUT_KAFKA_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "utils/RegexUtils.h"
#include "rdkafka.h"
#include "KafkaPool.h"
#include <atomic>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>

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
      : core::Processor(name, uuid),
        connection_pool_(5),
        logger_(logging::LoggerFactory<PublishKafka>::getLogger()),
        interrupted_(false) {
  }
  // Destructor
  virtual ~PublishKafka() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "PublishKafka";
  // Supported Properties
  static core::Property SeedBrokers;
  static core::Property Topic;
  static core::Property DeliveryGuarantee;
  static core::Property MaxMessageSize;
  static core::Property RequestTimeOut;
  static core::Property MessageTimeOut;
  static core::Property ClientName;
  static core::Property BatchSize;
  static core::Property TargetBatchPayloadSize;
  static core::Property AttributeNameRegex;
  static core::Property QueueBufferMaxTime;
  static core::Property QueueBufferMaxSize;
  static core::Property QueueBufferMaxMessage;
  static core::Property CompressCodec;
  static core::Property MaxFlowSegSize;
  static core::Property SecurityProtocol;
  static core::Property SecurityCA;
  static core::Property SecurityCert;
  static core::Property SecurityPrivateKey;
  static core::Property SecurityPrivateKeyPassWord;
  static core::Property KerberosServiceName;
  static core::Property KerberosPrincipal;
  static core::Property KerberosKeytabPath;
  static core::Property MessageKeyField;
  static core::Property DebugContexts;

  // Supported Relationships
  static core::Relationship Failure;
  static core::Relationship Success;

  // Message
   struct MessageResult {
    bool completed;
    bool is_error;
    rd_kafka_resp_err_t err_code;

    MessageResult()
        : completed(false)
        , is_error(false) {
    }
  };
  struct FlowFileResult {
    bool flow_file_error;
    std::vector<MessageResult> messages;

    FlowFileResult()
        : flow_file_error(false) {
    }
  };
  struct Messages {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<FlowFileResult> flow_files;
    bool interrupted;

    Messages()
        : interrupted(false) {
    }

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
            return message.completed;
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
    ReadCallback(uint64_t max_seg_size,
                 const std::string &key,
                 rd_kafka_topic_t *rkt,
                 rd_kafka_t *rk,
                 const std::shared_ptr<core::FlowFile> flowFile,
                 utils::Regex &attributeNameRegex,
                 std::shared_ptr<Messages> messages,
                 size_t flow_file_index)
        : max_seg_size_(max_seg_size),
          key_(key),
          rkt_(rkt),
          rk_(rk),
          flowFile_(flowFile),
          messages_(std::move(messages)),
          flow_file_index_(flow_file_index),
          attributeNameRegex_(attributeNameRegex) {
      flow_size_ = flowFile_->getSize();
      status_ = 0;
      read_size_ = 0;
      hdrs = nullptr;
    }

    ~ReadCallback() {
      if (hdrs) {
        rd_kafka_headers_destroy(hdrs);
      }
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      if (max_seg_size_ == 0U || flow_size_ < max_seg_size_) {
        max_seg_size_ = flow_size_;
      }
      std::vector<unsigned char> buffer;
      buffer.reserve(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      rd_kafka_resp_err_t err;

      for (auto kv : flowFile_->getAttributes()) {
        if(attributeNameRegex_.match(kv.first)) {
          if (!hdrs) {
            hdrs = rd_kafka_headers_new(8);
          }
          err = rd_kafka_header_add(hdrs, kv.first.c_str(), kv.first.size(), kv.second.c_str(), kv.second.size());
        }
      }

      size_t segment_num = 0U;
      while (read_size_ < flow_size_) {
        int readRet = stream->read(&buffer[0], max_seg_size_);
        if (readRet < 0) {
          status_ = -1;
          return read_size_;
        }
        if (readRet > 0) {
          messages_->modifyResult(flow_file_index_, [](FlowFileResult& flow_file) {
            flow_file.messages.resize(flow_file.messages.size() + 1);
          });
          auto messages_copy = this->messages_;
          auto flow_file_index_copy = this->flow_file_index_;
          auto callback = std::unique_ptr<std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>>(
              new std::function<void(rd_kafka_t*, const rd_kafka_message_t*)>(
                [messages_copy, flow_file_index_copy, segment_num](rd_kafka_t* /*rk*/, const rd_kafka_message_t* rkmessage) {
                  messages_copy->modifyResult(flow_file_index_copy, [segment_num, rkmessage](FlowFileResult& flow_file) {
                    auto& message = flow_file.messages.at(segment_num);
                    message.completed = true;
                    message.err_code = rkmessage->err;
                    message.is_error = message.err_code != 0;
                  });
                }));
          if (hdrs) {
            rd_kafka_headers_t *hdrs_copy;
            hdrs_copy = rd_kafka_headers_copy(hdrs);
            err = rd_kafka_producev(rk_, RD_KAFKA_V_RKT(rkt_), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(&buffer[0], readRet),
                                    RD_KAFKA_V_HEADERS(hdrs_copy), RD_KAFKA_V_KEY(key_.c_str(), key_.size()), RD_KAFKA_V_OPAQUE(callback.release()), RD_KAFKA_V_END);
            if (err) {
              rd_kafka_headers_destroy(hdrs_copy);
            }
          } else {
            err = rd_kafka_producev(rk_, RD_KAFKA_V_RKT(rkt_), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(&buffer[0], readRet),
                                    RD_KAFKA_V_KEY(key_.c_str(), key_.size()), RD_KAFKA_V_OPAQUE(callback.release()), RD_KAFKA_V_END);
          }
          if (err) {
            rd_kafka_resp_err_t resp_err = rd_kafka_last_error();
            messages_->modifyResult(flow_file_index_, [segment_num, resp_err](FlowFileResult& flow_file) {
              auto& message = flow_file.messages.at(segment_num);
              message.completed = true;
              message.is_error = true;
              message.err_code = resp_err;
            });
            status_ = -1;
            return read_size_;
          }
          read_size_ += readRet;
        } else {
          break;
        }
        segment_num++;
      }
      return read_size_;
    }

    uint64_t flow_size_;
    uint64_t max_seg_size_;
    std::string key_;
    rd_kafka_topic_t *rkt_;
    rd_kafka_t *rk_;
    rd_kafka_headers_t *hdrs;
    std::shared_ptr<core::FlowFile> flowFile_;
    std::shared_ptr<Messages> messages_;
    size_t flow_file_index_;
    int status_;
    int read_size_;
    utils::Regex& attributeNameRegex_;
  };

 public:

  virtual bool supportsDynamicProperties() override {
    return true;
  }

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  virtual void notifyStop() override;

 protected:

  bool configureNewConnection(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context);
  bool createNewTopic(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name);

 private:
  static void messageDeliveryCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque);

  std::shared_ptr<logging::Logger> logger_;

  KafkaPool connection_pool_;

  std::atomic<bool> interrupted_;
  std::mutex messages_mutex_;
  std::set<std::shared_ptr<Messages>> messages_set_;
};

REGISTER_RESOURCE(PublishKafka, "This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. "
                  "This message is optionally assigned a key by using the <Kafka Key> Property.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

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
#include "rdkafka.h"
#include "KafkaPool.h"
#include <regex>

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
        logger_(logging::LoggerFactory<PublishKafka>::getLogger()) {
    max_seg_size_ = -1;
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
  static core::Property ClientName;
  static core::Property BatchSize;
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

  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t max_seg_size, const std::string &key, rd_kafka_topic_t *rkt, rd_kafka_t *rk, const std::shared_ptr<core::FlowFile> flowFile, const std::regex &attributeNameRegex)
        : max_seg_size_(max_seg_size),
          key_(key),
          rkt_(rkt),
          rk_(rk),
          flowFile_(flowFile),
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
      if (flow_size_ < max_seg_size_)
        max_seg_size_ = flow_size_;
      std::vector<unsigned char> buffer;
      buffer.reserve(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      rd_kafka_resp_err_t err;

      for (auto kv : flowFile_->getAttributes()) {
        if (regex_match(kv.first, attributeNameRegex_)) {
          if (!hdrs) {
            hdrs = rd_kafka_headers_new(8);
          }
          err = rd_kafka_header_add(hdrs, kv.first.c_str(), kv.first.size(), kv.second.c_str(), kv.second.size());
        }
      }

      while (read_size_ < flow_size_) {
        int readRet = stream->read(&buffer[0], max_seg_size_);
        if (readRet < 0) {
          status_ = -1;
          return read_size_;
        }
        if (readRet > 0) {
          if (hdrs) {
            rd_kafka_headers_t *hdrs_copy;
            hdrs_copy = rd_kafka_headers_copy(hdrs);
            err = rd_kafka_producev(rk_, RD_KAFKA_V_RKT(rkt_), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(&buffer[0], readRet),
                                    RD_KAFKA_V_HEADERS(hdrs_copy), RD_KAFKA_V_KEY(key_.c_str(), key_.size()), RD_KAFKA_V_END);
            if (err) {
              rd_kafka_headers_destroy(hdrs_copy);
            }
          } else {
            err = rd_kafka_producev(rk_, RD_KAFKA_V_RKT(rkt_), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA), RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(&buffer[0], readRet),
                                    RD_KAFKA_V_KEY(key_.c_str(), key_.size()), RD_KAFKA_V_END);
          }
          if (err) {
            status_ = -1;
            return read_size_;
          }
          read_size_ += readRet;
        } else {
          break;
        }
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
    int status_;
    int read_size_;
    std::regex attributeNameRegex_;
  };

 public:

  virtual bool supportsDynamicProperties() {
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

 protected:

  bool configureNewConnection(const std::shared_ptr<KafkaConnection> &conn, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::FlowFile> &ff);

 private:
  std::shared_ptr<logging::Logger> logger_;

  KafkaPool connection_pool_;

//  rd_kafka_conf_t *conf_;
  //rd_kafka_t *rk_;
  //1rd_kafka_topic_conf_t *topic_conf_;
  //rd_kafka_topic_t *rkt_;
  //std::string topic_;
  uint64_t max_seg_size_;
  std::regex attributeNameRegex;
};

REGISTER_RESOURCE(PublishKafka, "This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. "
                  "This message is optionally assigned a key by using the <Kafka Key> Property.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

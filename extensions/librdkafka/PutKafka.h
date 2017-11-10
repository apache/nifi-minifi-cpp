/**
 * @file PutKafka.h
 * PutKafka class declaration
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
#include "rdkafka.h"

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

// PutKafka Class
class PutKafka: public core::Processor {
public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit PutKafka(std::string name, uuid_t uuid = NULL) :
      core::Processor(name, uuid), logger_(logging::LoggerFactory<PutKafka>::getLogger()) {
    conf_ = nullptr;
    rk_ = nullptr;
    topic_conf_ = nullptr;
    rkt_ = nullptr;
  }
  // Destructor
  virtual ~PutKafka() {
    if (rk_)
      rd_kafka_flush(rk_, 10*1000); /* wait for max 10 seconds */
    if (rkt_)
      rd_kafka_topic_destroy(rkt_);
    if (rk_)
      rd_kafka_destroy(rk_);
  }
  // Processor Name
  static constexpr char const* ProcessorName = "PutKafka";
  // Supported Properties
  static core::Property SeedBrokers;
  static core::Property Topic;
  static core::Property DeliveryGuarantee;
  static core::Property MaxMessageSize;
  static core::Property RequestTimeOut;
  static core::Property ClientName;
  static core::Property BatchSize;
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

  // Supported Relationships
  static core::Relationship Failure;
  static core::Relationship Success;

  // Nest Callback Class for read stream
  class ReadCallback: public InputStreamCallback {
  public:
    ReadCallback(uint64_t flow_size, uint64_t max_seg_size, const std::string &key, rd_kafka_topic_t *rkt) :
        flow_size_(flow_size), max_seg_size_(max_seg_size), key_(key), rkt_(rkt) {
      status_ = 0;
      read_size_ = 0;
    }
    ~ReadCallback() {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      if (flow_size_ < max_seg_size_)
        max_seg_size_ = flow_size_;
      std::vector<unsigned char> buffer;
      buffer.reserve(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      while (read_size_ < flow_size_) {
        int readRet = stream->read(&buffer[0], max_seg_size_);
        if (readRet < 0) {
          status_ = -1;
          return read_size_;
        }
        if (readRet > 0) {
          if (rd_kafka_produce(rkt_, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, &buffer[0], readRet, key_.c_str(), key_.size(), NULL) == -1) {
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
    int status_;
    int read_size_;
  };

public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  // OnTrigger method, implemented by NiFi PutKafka
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  }
  // OnTrigger method, implemented by NiFi PutKafka
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
  // Initialize, over write by NiFi PutKafka
  virtual void initialize(void);

protected:

private:
  std::shared_ptr<logging::Logger> logger_;
  rd_kafka_conf_t *conf_;
  rd_kafka_t *rk_;
  rd_kafka_topic_conf_t *topic_conf_;
  rd_kafka_topic_t *rkt_;
  std::string topic_;
  uint64_t max_seg_size_;
};

REGISTER_RESOURCE (PutKafka);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

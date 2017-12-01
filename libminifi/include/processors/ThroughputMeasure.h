/**
 * @file ThroughputMeasure.h
 * ThroughputMeasure class declaration
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
#ifndef _THROUGHPUT_MEASURE_
#define _THROUGHPUT_MEASURE_

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

#include <chrono>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! ThroughputMeasure Class
class ThroughputMeasure : public core::Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  explicit ThroughputMeasure(std::string name, uuid_t uuid = nullptr)
      : Processor(name, uuid) {
    logger_ = logging::LoggerFactory<ThroughputMeasure>::getLogger();
  }

  ~ThroughputMeasure(){

  }
  //! Processor Name
  static constexpr char const* ProcessorName = "ThroughputMeasure";

  static core::Relationship Success;
  //! Default maximum bytes to read into an attribute
  static constexpr int DEFAULT_SIZE_LIMIT = 2 * 1024 * 1024;

  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &processContext, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
    start_ = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    count_ = 0;
    size_ = 0;
  }

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    do {
      auto ff = session->get();
      if (!ff) {
        break;
      }
      count_++;
      size_+= ff->getSize();

      if ((count_ % 250) == 0){
        auto now = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
        auto rate = size_ / ( now - start_ );
        rate *= 1000;
        logger_->log_info("Received %u for a rate of %u bytes per second",count_.load(), rate);
      }
      session->transfer(ff,Success);
    } while (true);
  }
  //! Initialize, over write by NiFi ThroughputMeasure
  void initialize(void){

  }

 protected:

 private:
  uint64_t start_;
  std::atomic<uint64_t> count_;
  std::atomic<uint64_t> size_;
  //! Logger
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(ThroughputMeasure);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

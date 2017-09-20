/**
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
#ifndef __GET_TCP_H__
#define __GET_TCP_H__

#include <atomic>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "concurrentqueue.h"
#include "utils/ThreadPool.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/state/metrics/MetricsBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class SocketAfterExecute : public utils::AfterExecute<int> {
 public:
  explicit SocketAfterExecute(std::atomic<bool> &running, const std::string &endpoint, std::map<std::string, std::future<int>*> *list, std::mutex *mutex)
      : running_(running.load()),
        endpoint_(endpoint),
        mutex_(mutex),
        list_(list) {
  }

  explicit SocketAfterExecute(SocketAfterExecute && other) {
  }

  ~SocketAfterExecute() {
  }

  virtual bool isFinished(const int &result) {

    if (result == -1 || result == 0 || !running_) {
      std::lock_guard<std::mutex> lock(*mutex_);
      list_->erase(endpoint_);
      return true;
    } else{
    return false;
    }
  }
  virtual bool isCancelled(const int &result) {
    if (!running_)
      return true;
    else
      return false;
  }

 protected:
  std::atomic<bool> running_;
  std::map<std::string, std::future<int>*> *list_;
  std::mutex *mutex_;
  std::string endpoint_;
};

class DataHandlerCallback : public OutputStreamCallback {
 public:
  DataHandlerCallback(uint8_t *message, size_t size)
      : message_(message),
        size_(size) {
  }

  virtual ~DataHandlerCallback() {

  }

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) {
    return stream->write(message_, size_);
  }

 private:
  uint8_t *message_;
  size_t size_;
};

class DataHandler {
 public:
  DataHandler(std::shared_ptr<core::ProcessSessionFactory> sessionFactory)
      : sessionFactory_(sessionFactory) {

  }
  static const char *SOURCE_ENDPOINT_ATTRIBUTE;

  int16_t handle(std::string source, uint8_t *message, size_t size, bool partial);

 private:
  std::shared_ptr<core::ProcessSessionFactory> sessionFactory_;

};

class GetTCPMetrics : public state::metrics::Metrics {
 public:
  GetTCPMetrics()
      : state::metrics::Metrics("GetTCPMetrics", 0) {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }

  GetTCPMetrics(std::string name, uuid_t uuid)
      : state::metrics::Metrics(name, uuid) {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }
  virtual ~GetTCPMetrics() {

  }
  virtual std::string getName() {
    return core::Connectable::getName();
  }

  virtual std::vector<state::metrics::MetricResponse> serialize() {
    std::vector<state::metrics::MetricResponse> resp;

    state::metrics::MetricResponse iter;
    iter.name = "OnTriggerInvocations";
    iter.value = std::to_string(iterations_.load());

    resp.push_back(iter);

    state::metrics::MetricResponse accepted_files;
    accepted_files.name = "AcceptedFiles";
    accepted_files.value = std::to_string(accepted_files_.load());

    resp.push_back(accepted_files);

    state::metrics::MetricResponse input_bytes;
    input_bytes.name = "InputBytes";
    input_bytes.value = std::to_string(input_bytes_.load());

    resp.push_back(input_bytes);

    return resp;
  }

 protected:
  friend class GetTCP;

  std::atomic<size_t> iterations_;
  std::atomic<size_t> accepted_files_;
  std::atomic<size_t> input_bytes_;

};

// GetTCP Class
class GetTCP : public core::Processor, public state::metrics::MetricsSource {
 public:
// Constructor
  /*!
   * Create a new processor
   */
  explicit GetTCP(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        connection_attempts_(3),
        reconnect_interval_(5000),
        receive_buffer_size_(16*1024*1024),
        stay_connected_(true),
        endOfMessageByte(13),
        running_(false),
        connection_attempt_limit_(3),
        concurrent_handlers_(2),
        logger_(logging::LoggerFactory<GetTCP>::getLogger()) {
    metrics_ = std::make_shared<GetTCPMetrics>();
  }
// Destructor
  virtual ~GetTCP() {
  }
// Processor Name
  static constexpr char const* ProcessorName = "GetTCP";

  // Supported Properties
  static core::Property EndpointList;
  static core::Property ConcurrentHandlers;
  static core::Property ReconnectInterval;
  static core::Property StayConnected;
  static core::Property ReceiveBufferSize;
  static core::Property ConnectionAttemptLimit;
  static core::Property EndOfMessageByte;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Partial;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  virtual void onSchedule(std::shared_ptr<core::ProcessContext> processContext, std::shared_ptr<core::ProcessSessionFactory> sessionFactory);

  void onSchedule(core::ProcessContext *processContext, core::ProcessSessionFactory *sessionFactory){
    throw std::exception();
  }
  /**
   * Execution trigger for the GetTCP Processor
   * @param context processor context
   * @param session processor session reference.
   */
  virtual void onTrigger(std::shared_ptr<core::ProcessContext> context, std::shared_ptr<core::ProcessSession> session);

  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session){
    throw std::exception();
  }

  // Initialize, over write by NiFi GetTCP
  virtual void initialize(void);

  int16_t getMetrics(std::vector<std::shared_ptr<state::metrics::Metrics>> &metric_vector);

 protected:

  virtual void notifyStop();

 private:

  std::function<int()> f_ex;

  std::atomic<bool> running_;

  std::unique_ptr<DataHandler> handler_;

  std::vector<std::string> endpoints;

  std::map<std::string, std::future<int>*> live_clients_;

  utils::ThreadPool<int> client_thread_pool_;

  moodycamel::ConcurrentQueue<std::unique_ptr<io::Socket>> socket_ring_buffer_;

  bool stay_connected_;

  uint16_t concurrent_handlers_;

  int8_t endOfMessageByte;

  int64_t reconnect_interval_;

  int64_t receive_buffer_size_;

  int8_t connection_attempts_;

  uint16_t connection_attempt_limit_;

  std::shared_ptr<GetTCPMetrics> metrics_;

  // Mutex for ensuring clients are running

  std::mutex mutex_;

// last listing time for root directory ( if recursive, we will consider the root
// as the top level time.

  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(GetTCP);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

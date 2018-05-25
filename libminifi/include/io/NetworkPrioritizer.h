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
#ifndef LIBMINIFI_INCLUDE_IO_NETWORKPRIORITIZER_H_
#define LIBMINIFI_INCLUDE_IO_NETWORKPRIORITIZER_H_

#include <iostream>
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class NetworkInterface;

class NetworkPrioritizer {
 public:

  virtual ~NetworkPrioritizer() {
  }

  virtual NetworkInterface &&getInterface(uint32_t size) = 0;

 protected:
  friend class NetworkInterface;
  virtual void reduce_tokens(uint32_t size) = 0;

};

class NetworkInterface {
 public:

  NetworkInterface() : prioritizer_(nullptr){
  }

  virtual ~NetworkInterface(){
  }

  explicit NetworkInterface(const std::string &ifc, const std::shared_ptr<NetworkPrioritizer> &prioritizer)
      : ifc_(ifc),
        prioritizer_(prioritizer) {
  }

  explicit NetworkInterface(const NetworkInterface &&other)
      : ifc_(std::move(other.ifc_)),
        prioritizer_(std::move(other.prioritizer_)) {
  }

  std::string getInterface() const {
    return ifc_;
  }
  void log_write(uint32_t size) {
    if (nullptr != prioritizer_) {
      prioritizer_->reduce_tokens(size);
    }
  }

  void log_read(uint32_t size) {
    if (nullptr != prioritizer_) {
      prioritizer_->reduce_tokens(size);
    }
  }

  NetworkInterface &operator=(const NetworkInterface &&other) {
    ifc_ = std::move(other.ifc_);
    prioritizer_ = std::move(other.prioritizer_);
    return *this;
  }
 private:
  friend class NetworkPrioritizer;
  std::string ifc_;
  std::shared_ptr<NetworkPrioritizer> prioritizer_;
};

class NetworkPrioritizerFactory {
 public:
  static std::shared_ptr<NetworkPrioritizerFactory> getInstance() {
    static std::shared_ptr<NetworkPrioritizerFactory> fa = std::make_shared<NetworkPrioritizerFactory>();
    return fa;
  }

  int setPrioritizer(const std::shared_ptr<NetworkPrioritizer> &prioritizer) {
    if (np_ != nullptr)
      return -1;
    np_ = prioritizer;
    return 0;
  }

  std::shared_ptr<NetworkPrioritizer> getPrioritizer() {
    return np_;
  }
 private:
  std::shared_ptr<NetworkPrioritizer> np_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_IO_NETWORKPRIORITIZER_H_ */

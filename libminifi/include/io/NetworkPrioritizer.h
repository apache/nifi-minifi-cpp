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
#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <utility>

namespace org::apache::nifi::minifi::io {

class NetworkInterface;

struct NetworkPrioritizer {
  virtual ~NetworkPrioritizer() noexcept = default;

 protected:
  friend class NetworkInterface;
  virtual void reduce_tokens(uint32_t size) = 0;
};

class NetworkInterface {
 public:
  NetworkInterface() = default;

  NetworkInterface(std::string ifc, std::shared_ptr<NetworkPrioritizer> prioritizer)
      : ifc_{std::move(ifc)}, prioritizer_{std::move(prioritizer)}
  { }

  NetworkInterface(const NetworkInterface &other) = default;
  NetworkInterface(NetworkInterface &&other) noexcept = default;

  virtual ~NetworkInterface() = default;

  std::string getInterface() const { return ifc_; }

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

  NetworkInterface &operator=(const NetworkInterface &other) = default;
  NetworkInterface &operator=(NetworkInterface &&other) = default;

 private:
  friend struct NetworkPrioritizer;

  std::string ifc_;
  std::shared_ptr<NetworkPrioritizer> prioritizer_;
};

}  // namespace org::apache::nifi::minifi::io

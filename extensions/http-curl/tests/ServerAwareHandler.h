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
#include <array>

#include "CivetServer.h"

class ServerAwareHandler: public CivetHandler {
 protected:
  void sleep_for(std::chrono::milliseconds time) {
    std::unique_lock<std::mutex> lock(mutex_);
    stop_signal_.wait_for(lock, time, [&] {return terminate_.load();});
  }

  bool isServerRunning() const {
    return !terminate_.load();
  }

  virtual std::string readPayload(struct mg_connection* conn) {
    std::string response;
    int readBytes;

    std::array<char, 1024> buffer;
    while ((readBytes = mg_read(conn, buffer.data(), buffer.size())) > 0) {
      response.append(buffer.data(), readBytes);
    }
    return response;
  }

 public:
  void stop() {
    terminate_ = true;
    stop_signal_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable stop_signal_;
  std::atomic_bool terminate_{false};
};

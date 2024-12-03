/**
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

#ifndef LIBMINIFI_INCLUDE_UTILS_CALLBACKTIMER_H_
#define LIBMINIFI_INCLUDE_UTILS_CALLBACKTIMER_H_

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class CallBackTimer {
 public:
  CallBackTimer(std::chrono::milliseconds interval, const std::function<void(void)>& func);
  ~CallBackTimer();

  void stop();

  void start();

  bool is_running() const;

 private:
  bool execute_;
  std::function<void(void)> func_;
  std::thread thd_;
  mutable std::mutex mtx_;
  mutable std::mutex cv_mtx_;
  std::condition_variable cv_;

  const std::chrono::milliseconds interval_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_UTILS_CALLBACKTIMER_H_


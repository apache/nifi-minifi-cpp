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

#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_set>

#include "utils/file/FileUtils.h"
#include "utils/Environment.h"
#include "utils/Id.h"
#include "utils/TimeUtil.h"

#ifdef WIN32
#include "date/tz.h"
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string putFileToDir(const std::string& dir_path, const std::string& file_name, const std::string& content) {
  std::string file_path(file::FileUtils::concat_path(dir_path, file_name));
  std::ofstream out_file(file_path, std::ios::binary | std::ios::out);
  if (out_file.is_open()) {
    out_file << content;
  }
  return file_path;
}

std::string getFileContent(const std::string& file_name) {
  std::ifstream file_handle(file_name, std::ios::binary | std::ios::in);
  assert(file_handle.is_open());
  std::string file_content{ (std::istreambuf_iterator<char>(file_handle)), (std::istreambuf_iterator<char>()) };
  return file_content;
}

Identifier generateUUID() {
  // TODO(hunyadi): Will make the Id generator manage lifetime using a unique_ptr and return a raw ptr on access
  static std::shared_ptr<utils::IdGenerator> id_generator = utils::IdGenerator::getIdGenerator();
  return id_generator->generate();
}

class ManualClock : public timeutils::SteadyClock {
 public:
  [[nodiscard]] std::chrono::milliseconds timeSinceEpoch() const override {
    std::lock_guard lock(mtx_);
    return time_;
  }

  [[nodiscard]] std::chrono::time_point<std::chrono::steady_clock> now() const override {
    return std::chrono::steady_clock::time_point{timeSinceEpoch()};
  }

  void advance(std::chrono::milliseconds elapsed_time) {
    if (elapsed_time.count() < 0) {
      throw std::logic_error("A steady clock can only be advanced forward");
    }
    std::lock_guard lock(mtx_);
    time_ += elapsed_time;
    for (auto* cv : cvs_) {
      cv->notify_all();
    }
  }

  bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time, const std::function<bool()>& pred) override {
    std::chrono::milliseconds now;
    {
      std::unique_lock lock(mtx_);
      now = time_;
      cvs_.insert(&cv);
    }
    cv.wait_for(lck, time - now, [&] {
      now = timeSinceEpoch();
      return now >= time || pred();
    });
    {
      std::unique_lock lock(mtx_);
      cvs_.erase(&cv);
    }
    return pred();
  }

 private:
  mutable std::mutex mtx_;
  std::unordered_set<std::condition_variable*> cvs_;
  std::chrono::milliseconds time_{0};
};

#ifdef WIN32
// The tzdata location is set as a global variable in date-tz library
// We need to set it from from libminifi to effect calls made from libminifi (on Windows)
void dateSetInstall(const std::string& install);
#endif

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

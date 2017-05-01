/**
 * @file Repository 
 * Repository class declaration
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
#ifndef __REPOSITORY_H__
#define __REPOSITORY_H__

#include <ftw.h>
#include <uuid/uuid.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/Property.h"
#include "ResourceClaim.h"
#include "io/Serializable.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class Repository : public CoreComponent {
 public:
  /*
   * Constructor for the repository
   */
  Repository(std::string repo_name, std::string directory,
             int64_t maxPartitionMillis, int64_t maxPartitionBytes,
             uint64_t purgePeriod)
      : CoreComponent(repo_name),
        thread_() {
    directory_ = directory;
    max_partition_millis_ = maxPartitionMillis;
    max_partition_bytes_ = maxPartitionBytes;
    purge_period_ = purgePeriod;
    running_ = false;
    repo_full_ = false;
  }

  // Destructor
  virtual ~Repository() {
    stop();
  }

  // initialize
  virtual bool initialize(std::shared_ptr<Configure> configure) {
    return true;
  }
  // Put
  virtual bool Put(std::string key, uint8_t *buf, int bufLen) {
    return true;
  }
  // Delete
  virtual bool Delete(std::string key) {
    return true;
  }

  virtual bool Get(std::string key, std::string &value) {
    return true;
  }

  // Run function for the thread
  virtual void run() {
    // no op
  }
  // Start the repository monitor thread
  virtual void start();
  // Stop the repository monitor thread
  virtual void stop();
  // whether the repo is full
  virtual bool isFull() {
    return repo_full_;
  }
  // whether the repo is enable
  virtual bool isRunning() {
    return running_;
  }
  uint64_t incrementSize(const char *fpath, const struct stat *sb,
                         int typeflag) {
    return (repo_size_ += sb->st_size);
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Repository(const Repository &parent) = delete;
  Repository &operator=(const Repository &parent) = delete;

 protected:
  // Mutex for protection
  std::mutex mutex_;
  // repository directory
  std::string directory_;
  // max db entry life time
  int64_t max_partition_millis_;
  // max db size
  int64_t max_partition_bytes_;
  // purge period
  uint64_t purge_period_;
  // thread
  std::thread thread_;
  // whether the monitoring thread is running for the repo while it was enabled
  bool running_;
  // whether stop accepting provenace event
  std::atomic<bool> repo_full_;
  // repoSize
  uint64_t repoSize();
  // size of the directory
  std::atomic<uint64_t> repo_size_;
  // Run function for the thread
  void threadExecutor() {
    run();
  }
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif

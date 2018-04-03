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
#ifndef __META_INFO_H__
#define __META_INFO_H__

#include <algorithm>
#include <sstream>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <set>
#include <stdlib.h>
#include <math.h>
#include "utils/StringUtils.h"
#include "core/FlowFile.h"
#include "core/state/metrics/DeviceInformation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// MetaInfo Class
class MetaInfo {

 public:
  // Constructor
  /*!
   * Create a new meta info
   */
  explicit MetaInfo(const std::string &name, const std::string &value)
      : name_(name),
        value_(value) {
  }

  // Destructor
  virtual ~MetaInfo() {
  }

  // Get Name for the meta info
  std::string getName() {
    return name_;
  }
  // Get value for the meta info, overwritten by specific child meta info
  virtual std::string getValue(const std::shared_ptr<core::FlowFile> &flow) {
    return value_;
  }

 protected:
  // Name
  std::string name_;
  // Value
  std::string value_;

 private:
};

#ifdef MINIFI_VERSION
#define MINIFI_VERSION_STRINGIZE(s) #s
#define MINIFI_VERSION_MKSTRING(s) MINIFI_VERSION_STRINGIZE(s)
#define MINIFI_VERSION_STRING MINIFI_VERSION_MKSTRING(MINIFI_VERSION)
#endif

// Version Meta Info
class VersionMetaInfo : public MetaInfo {

public:
  // Constructor
  /*!
   * Create a new meta info
   */
  VersionMetaInfo()
      : MetaInfo("agent.version", "") {
    char version[120];
    sprintf(version, "%s", MINIFI_VERSION_STRING);
    value_ = version;
  }

  // Get value for the meta info
  virtual std::string getValue(const std::shared_ptr<core::FlowFile> &flow) {
    return value_;
  }
};


// MetaInfo Container Class
class MetaInfoContainer {

 public:
  // Constructor
  /*!
   * Create a new meta info container
   */
  MetaInfoContainer(const std::shared_ptr<Configure> &configure)
     : config_(configure) {
  }

  // Destructor
  virtual ~MetaInfoContainer() {
  }

  void addMetaInfo(std::unique_ptr<MetaInfo> meta_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    registered_info_[meta_info->getName()] = std::move(meta_info);
  }

  // get meta info base on the meta info name
  bool getValue(std::string &name, const std::shared_ptr<core::FlowFile> &flow,
      std::string &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registered_info_.find(name);

    if (it != registered_info_.end()) {
      std::unique_ptr<MetaInfo> &info = it->second;
      value = info->getValue(flow);
      return true;
    } else {
      return false;
    }
  }

  // apply all registered meta info to the flow
  void applyMetaInfo(const std::shared_ptr<core::FlowFile> &flow) {
      std::lock_guard<std::mutex> lock(mutex_);
      for (std::map<std::string, std::unique_ptr<MetaInfo>>::iterator it= registered_info_.begin(); it != registered_info_.end(); ++it) {
        std::unique_ptr<MetaInfo> &info = it->second;
        std::string key = it->first;
        std::string value = info->getValue(flow);
        flow->setAttribute(key, value);
      }
    }

 protected:

 private:
  std::map<std::string, std::unique_ptr<MetaInfo>> registered_info_;
  std::mutex mutex_;
  std::shared_ptr<Configure> config_;
};


} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

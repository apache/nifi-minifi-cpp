/**
 * @file AppendHostInfo.h
 * AppendHostInfo class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_APPENDHOSTINFO_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_APPENDHOSTINFO_H_

#include <memory>
#include <string>
#include <optional>
#include <regex>
#include <shared_mutex>

#include "core/Property.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class AppendHostInfo : public core::Processor {
 public:
  static constexpr const char* REFRESH_POLICY_ON_TRIGGER = "On every trigger";
  static constexpr const char* REFRESH_POLICY_ON_SCHEDULE = "On schedule";

  explicit AppendHostInfo(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid),
        logger_(logging::LoggerFactory<AppendHostInfo>::getLogger()),
        refresh_on_trigger_(false) {
  }
  virtual ~AppendHostInfo() = default;
  static constexpr char const* ProcessorName = "AppendHostInfo";

  EXTENSIONAPI static core::Property InterfaceNameFilter;
  EXTENSIONAPI static core::Property HostAttribute;
  EXTENSIONAPI static core::Property IPAttribute;
  EXTENSIONAPI static core::Property RefreshPolicy;

  EXTENSIONAPI static core::Relationship Success;

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;
  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;
  void initialize() override;

 protected:
  virtual void refreshHostInfo();

 private:
  std::shared_mutex shared_mutex_;
  std::shared_ptr<logging::Logger> logger_;
  std::string hostname_attribute_name_;
  std::string ipaddress_attribute_name_;
  std::optional<std::regex> interface_name_filter_;
  bool refresh_on_trigger_;

  std::string hostname_;
  std::optional<std::string> ipaddresses_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_APPENDHOSTINFO_H_

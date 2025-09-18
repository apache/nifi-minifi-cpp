/**
 * @file MetadataWalker.h
 * MetadataWalker class declaration
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

#include <Windows.h>
#include <winevt.h>
#include <codecvt>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <utility>

#include "core/Core.h"
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "WindowsEventLog.h"

#include "concurrentqueue.h"
#include "pugixml.hpp"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::wel {

/**
 * Defines a tree walker for the XML input
 *
 */
class MetadataWalker : public pugi::xml_tree_walker {
 public:
  MetadataWalker(const WindowsEventLogMetadata& windows_event_log_metadata, std::string log_name, bool update_xml, bool resolve,
      std::function<bool(std::string_view)> sid_matcher,
      std::function<std::string(std::string)> user_id_to_username_fn)
      : windows_event_log_metadata_(windows_event_log_metadata),
        log_name_(std::move(log_name)),
        sid_matcher_(std::move(sid_matcher)),
        update_xml_(update_xml),
        resolve_(resolve),
        user_id_to_username_fn_(std::move(user_id_to_username_fn)) {
  }

  bool for_each(pugi::xml_node &node) override;
  [[nodiscard]] std::map<std::string, std::string> getFieldValues() const;
  [[nodiscard]] std::map<std::string, std::string> getIdentifiers() const;
  [[nodiscard]] std::string getMetadata(Metadata metadata) const;

 private:
  static std::vector<std::string> getIdentifiers(const std::string &text);
  template<typename Fn>
  requires std::is_convertible_v<std::invoke_result_t<Fn, std::string>, std::string>
  void updateText(pugi::xml_node &node, const std::string &field_name, Fn &&fn);
  template<typename Fn>
  requires std::is_convertible_v<std::invoke_result_t<Fn, std::string>, std::string>
  void updateAttributeValue(pugi::xml_attribute &node, const std::string &field_name, Fn &&fn);

  const WindowsEventLogMetadata& windows_event_log_metadata_;
  const std::string log_name_;
  std::function<bool(std::string_view)> sid_matcher_;
  const bool update_xml_;
  const bool resolve_;
  std::function<std::string(const std::string&)> user_id_to_username_fn_;
  std::map<std::string, std::string> metadata_;
  std::map<std::string, std::string> fields_values_;
  std::map<std::string, std::string> replaced_identifiers_;
};

}  // namespace org::apache::nifi::minifi::wel

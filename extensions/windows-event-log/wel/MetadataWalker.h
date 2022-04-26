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

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/Core.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "utils/OsUtils.h"
#include "FlowFileRecord.h"
#include "WindowsEventLog.h"

#include "concurrentqueue.h"
#include "pugixml.hpp"
#include "utils/RegexUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {



/**
 * Defines a tree walker for the XML input
 *
 */
class MetadataWalker : public pugi::xml_tree_walker {
 public:
  MetadataWalker(const WindowsEventLogMetadata& windows_event_log_metadata, const std::string &log_name, bool update_xml, bool resolve, const std::string &regex = "")
      : windows_event_log_metadata_(windows_event_log_metadata),
        log_name_(log_name),
        regex_(regex),
        regex_str_(regex),
        update_xml_(update_xml),
        resolve_(resolve) {
  }

  /**
   * Overloaded function to visit a node
   * @param node Node that we are visiting.
   */
  bool for_each(pugi::xml_node &node) override;

  static std::string updateXmlMetadata(const std::string &xml, EVT_HANDLE metadata_ptr, EVT_HANDLE event_ptr, bool update_xml, bool resolve, const std::string &regex = "");

  std::map<std::string, std::string> getFieldValues() const;

  std::map<std::string, std::string> getIdentifiers() const;

  std::string getMetadata(METADATA metadata) const;

 private:
  std::vector<std::string> getIdentifiers(const std::string &text) const;

  static std::string getString(const std::map<std::string, std::string> &map, const std::string &field) {
    auto srch = map.find(field);
    if (srch != std::end(map)) {
      return srch->second;
    }
    return "N/A";
  }

  static std::string to_string(const wchar_t* pChar);

  /**
   * Updates text within the XML representation
   */
  void updateText(pugi::xml_node &node, const std::string &field_name, std::function<std::string(const std::string &)> &&fn);

  const WindowsEventLogMetadata& windows_event_log_metadata_;
  std::string log_name_;
  utils::Regex regex_;
  std::string regex_str_;
  bool update_xml_;
  bool resolve_;
  std::map<std::string, std::string> metadata_;
  std::map<std::string, std::string> fields_values_;
  std::map<std::string, std::string> replaced_identifiers_;
};

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

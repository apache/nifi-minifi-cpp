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

#pragma once

#include <string>

#include "rapidjson/document.h"
#include "NetworkInterfaceInfo.h"
#include "utils/net/DNS.h"

namespace org::apache::nifi::minifi::utils {

class OpenTelemetryLogDataModel {
 public:
  static void appendEventInformation(rapidjson::Document& root, const std::string& event_identifier) {
    rapidjson::Value name;
    name.SetString(event_identifier.c_str(), gsl::narrow<rapidjson::SizeType>(event_identifier.length()), root.GetAllocator());
    root.AddMember("Name", name, root.GetAllocator());
    root.AddMember("Timestamp", rapidjson::Value().SetInt64(std::time(0)), root.GetAllocator());
  }

  static void appendHostInformation(rapidjson::Document& root) {
    root.AddMember("Resource", rapidjson::Value{ rapidjson::kObjectType }, root.GetAllocator());
    appendHostName(root["Resource"], root.GetAllocator());
    appendIPAddresses(root["Resource"], root.GetAllocator());
  }

  static void appendBody(rapidjson::Document& root) {
    root.AddMember("Body", rapidjson::Value{ rapidjson::kObjectType }, root.GetAllocator());
  }

 private:
  static void appendHostName(rapidjson::Value& resource, rapidjson::Document::AllocatorType& allocator) {
    std::string hostname = utils::net::getMyHostName();
    rapidjson::Value hostname_value;
    hostname_value.SetString(hostname.c_str(), gsl::narrow<rapidjson::SizeType>(hostname.length()), allocator);
    resource.AddMember("host.hostname", hostname_value, allocator);
  }

  static void appendIPAddresses(rapidjson::Value& resource, rapidjson::Document::AllocatorType& alloc) {
    resource.AddMember("host.ip", rapidjson::Value{ rapidjson::kObjectType }, alloc);
    rapidjson::Value& ip = resource["host.ip"];
    auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos();
    for (const auto& network_interface_info : network_interface_infos) {
      rapidjson::Value interface_name(network_interface_info.getName().c_str(), gsl::narrow<rapidjson::SizeType>(network_interface_info.getName().length()), alloc);
      rapidjson::Value interface_address_array(rapidjson::kArrayType);
      for (auto& ip_v4_address : network_interface_info.getIpV4Addresses()) {
        rapidjson::Value address_value(ip_v4_address.c_str(), gsl::narrow<rapidjson::SizeType>(ip_v4_address.length()), alloc);
        interface_address_array.PushBack(address_value.Move(), alloc);
      }
      for (auto& ip_v6_address : network_interface_info.getIpV6Addresses()) {
        rapidjson::Value address_value(ip_v6_address.c_str(), gsl::narrow<rapidjson::SizeType>(ip_v6_address.length()), alloc);
        interface_address_array.PushBack(address_value.Move(), alloc);
      }
      ip.AddMember(interface_name, interface_address_array, alloc);
    }
  }
};

}  // namespace org::apache::nifi::minifi::utils

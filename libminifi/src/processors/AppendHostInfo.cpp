/**
 * @file AppendHostInfo.cpp
 * AppendHostInfo class implementation
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
#include "processors/AppendHostInfo.h"
#define __USE_POSIX
#include <limits.h>
#include <string.h>
#include <memory>
#include <string>
#include <set>
#include "core/ProcessContext.h"
#include "core/Property.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "io/ClientSocket.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#ifndef WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

core::Property AppendHostInfo::InterfaceName("Network Interface Name", "Network interface from which to read an IP v4 address", "eth0");
core::Property AppendHostInfo::HostAttribute("Hostname Attribute", "Flowfile attribute to used to record the agent's hostname", "source.hostname");
core::Property AppendHostInfo::IPAttribute("IP Attribute", "Flowfile attribute to used to record the agent's IP address", "source.ipv4");
core::Relationship AppendHostInfo::Success("success", "success operational on the flow record");

void AppendHostInfo::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(InterfaceName);
  properties.insert(HostAttribute);
  properties.insert(IPAttribute);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void AppendHostInfo::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::shared_ptr<core::FlowFile> flow = session->get();
  if (!flow)
    return;

  // Get Hostname

  std::string hostAttribute = "";
  context->getProperty(HostAttribute.getName(), hostAttribute);
  flow->addAttribute(hostAttribute.c_str(), org::apache::nifi::minifi::io::Socket::getMyHostName());

  // Get IP address for the specified interface
  std::string iface;
  context->getProperty(InterfaceName.getName(), iface);
  // Confirm the specified interface name exists on this device
#ifndef WIN32
  if (if_nametoindex(iface.c_str()) != 0) {
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    // Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    // Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    std::string ipAttribute;
    context->getProperty(IPAttribute.getName(), ipAttribute);
    flow->addAttribute(ipAttribute.c_str(), inet_ntoa(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr));
  }
#endif

  // Transfer to the relationship
  session->transfer(flow, Success);
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

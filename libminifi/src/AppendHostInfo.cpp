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
#include <set>
#include <sys/time.h>
#include <string.h>
#include "AppendHostInfo.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#define __USE_POSIX
#include <limits.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

const std::string AppendHostInfo::ProcessorName("AppendHostInfo");
Property AppendHostInfo::InterfaceName("Network Interface Name", "Network interface from which to read an IP v4 address", "eth0");
Property AppendHostInfo::HostAttribute("Hostname Attribute", "Flowfile attribute to used to record the agent's hostname", "source.hostname");
Property AppendHostInfo::IPAttribute("IP Attribute", "Flowfile attribute to used to record the agent's IP address", "source.ipv4");
Relationship AppendHostInfo::Success("success", "success operational on the flow record");

void AppendHostInfo::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(InterfaceName);
	properties.insert(HostAttribute);
	properties.insert(IPAttribute);
	setSupportedProperties(properties);

	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void AppendHostInfo::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flow = session->get();
	if (!flow)
	  return;

	//Get Hostname
	char hostname[HOST_NAME_MAX];
	hostname[HOST_NAME_MAX-1] = '\0'; 
	gethostname(hostname, HOST_NAME_MAX-1);
	struct hostent* h;
	h = gethostbyname(hostname);
  std::string hostAttribute;
  context->getProperty(HostAttribute.getName(), hostAttribute);
	flow->addAttribute(hostAttribute.c_str(), h->h_name);

	//Get IP address for the specified interface
  std::string iface;
	context->getProperty(InterfaceName.getName(), iface);
  //Confirm the specified interface name exists on this device
  if (if_nametoindex(iface.c_str()) != 0){
    struct ifreq ifr;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    //Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    //Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name , iface.c_str(), IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    std::string ipAttribute;
    context->getProperty(IPAttribute.getName(), ipAttribute);
    flow->addAttribute(ipAttribute.c_str(), inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
  }

	// Transfer to the relationship
	session->transfer(flow, Success);
}

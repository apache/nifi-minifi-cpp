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
#include "core/FlowConfiguration.h"
#include "core/logging/LoggerConfiguration.h"
#include "COAPLoader.h"

#ifdef WIN32
#include <winsock2.h>
#endif

bool COAPObjectFactory::added = core::FlowConfiguration::add_static_func("createCOAPFactory");

bool COAPObjectFactoryInitializer::initialize() {
#ifdef WIN32
  static WSADATA s_wsaData;
  int iWinSockInitResult = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
  if (iWinSockInitResult != 0) {
    logging::LoggerFactory<COAPObjectFactoryInitializer>::getLogger()->log_error("WSAStartup failed with error %d", iWinSockInitResult);
    return false;
  } else {
    return true;
  }
#else
  return true;
#endif
}

void COAPObjectFactoryInitializer::deinitialize() {
#ifdef WIN32
  WSACleanup();
#endif
}

extern "C" {



void *createCOAPFactory(void) {
  return new COAPObjectFactory();
}

}

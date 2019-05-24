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
#include "SFTPLoader.h"
#include "core/FlowConfiguration.h"
#include "client/SFTPClient.h"

bool SFTPFactoryInitializer::initialize() {
  if (libssh2_init(0) != 0) {
    return false;
  }
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    libssh2_exit();
    return false;
  }
  return true;
}

void SFTPFactoryInitializer::deinitialize() {
  curl_global_cleanup();
  libssh2_exit();
}


bool SFTPFactory::added = core::FlowConfiguration::add_static_func("createSFTPFactory");

extern "C" {

void *createSFTPFactory(void) {
  return new SFTPFactory();
}

}

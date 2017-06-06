/**
 * @file Site2SitePeer.cpp
 * Site2SitePeer class implementation
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
#include "Site2SitePeer.h"
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <random>
#include <memory>
#include <iostream>
#include "io/ClientSocket.h"
#include "io/validation.h"
#include "FlowController.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

bool Site2SitePeer::Open() {
  if (IsNullOrEmpty(host_))
    return false;

  if (stream_->initialize() < 0)
    return false;

  uint16_t data_size = sizeof MAGIC_BYTES;

  if (stream_->writeData(reinterpret_cast<uint8_t *>(const_cast<char*>(MAGIC_BYTES)), data_size) != data_size) {
    return false;
  }

  return true;
}

void Site2SitePeer::Close() {
  if (stream_ != nullptr)
    stream_->closeStream();
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

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

#pragma once

#include <memory>
#include <string>

#include "OutputStream.h"
#include "core/Core.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi::io {

class WriteArchiveStream : public OutputStream {
 public:
  virtual bool newEntry(const std::string& name, size_t size) = 0;
};

class WriteArchiveStreamProvider : public core::CoreComponent {
 public:
  using CoreComponent::CoreComponent;
  virtual std::unique_ptr<WriteArchiveStream> createStream(int compress_level, const std::string& compress_format,
      std::shared_ptr<OutputStream> sink, std::shared_ptr<core::logging::Logger> logger) = 0;
};

}  // namespace org::apache::nifi::minifi::io

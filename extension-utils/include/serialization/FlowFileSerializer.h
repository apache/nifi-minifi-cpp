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
#include <utility>
#include <functional>
#include "io/StreamCallback.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class OutputStream;

} /* namespace io */

namespace core {

class FlowFile;

} /* namespace core */

class FlowFileSerializer {
 public:
  using FlowFileReader = std::function<int64_t(const std::shared_ptr<core::FlowFile>&, const io::InputStreamCallback&)>;

  explicit FlowFileSerializer(FlowFileReader reader) : reader_(std::move(reader)) {}

  virtual int64_t serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) = 0;

  virtual ~FlowFileSerializer() = default;

 protected:
  FlowFileReader reader_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

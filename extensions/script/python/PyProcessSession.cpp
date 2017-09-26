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

#include <memory>

#include <pybind11/embed.h>

#include "PyProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;
namespace core = org::apache::nifi::minifi::core;

PyProcessSession::PyProcessSession(std::shared_ptr<core::ProcessSession> session)
    : session_(std::move(session)) {
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::get() {
  auto flow_file = session_->get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  return std::make_shared<script::ScriptFlowFile>(flow_file);
}

void PyProcessSession::transfer(std::shared_ptr<script::ScriptFlowFile> flow_file,
                                core::Relationship relationship) {
  session_->transfer(flow_file->getFlowFile(), relationship);
}

void PyProcessSession::read(std::shared_ptr<script::ScriptFlowFile> flow_file,
                            py::object input_stream_callback) {
  PyInputStreamCallback py_callback(input_stream_callback);
  session_->read(flow_file->getFlowFile(), &py_callback);
}

void PyProcessSession::write(std::shared_ptr<script::ScriptFlowFile> flow_file,
                             py::object output_stream_callback) {
  PyOutputStreamCallback py_callback(output_stream_callback);
  session_->write(flow_file->getFlowFile(), &py_callback);
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::create() {
  return std::make_shared<script::ScriptFlowFile>(session_->create());
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::create(std::shared_ptr<script::ScriptFlowFile> flow_file) {
  return std::make_shared<script::ScriptFlowFile>(session_->create(flow_file->getFlowFile()));
}

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

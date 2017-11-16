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
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = session_->get();

  if (flow_file == nullptr) {
    return nullptr;
  }

  auto result = std::make_shared<script::ScriptFlowFile>(flow_file);
  flow_files_.push_back(result);

  return result;
}

void PyProcessSession::transfer(std::shared_ptr<script::ScriptFlowFile> script_flow_file,
                                core::Relationship relationship) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  session_->transfer(flow_file, relationship);
}

void PyProcessSession::read(std::shared_ptr<script::ScriptFlowFile> script_flow_file,
                            py::object input_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  PyInputStreamCallback py_callback(input_stream_callback);
  session_->read(flow_file, &py_callback);
}

void PyProcessSession::write(std::shared_ptr<script::ScriptFlowFile> script_flow_file,
                             py::object output_stream_callback) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto flow_file = script_flow_file->getFlowFile();

  if (!flow_file) {
    throw std::runtime_error("Access of FlowFile after it has been released");
  }

  PyOutputStreamCallback py_callback(output_stream_callback);
  session_->write(flow_file, &py_callback);
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::create() {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  auto result = std::make_shared<script::ScriptFlowFile>(session_->create());
  flow_files_.push_back(result);
  return result;
}

std::shared_ptr<script::ScriptFlowFile> PyProcessSession::create(std::shared_ptr<script::ScriptFlowFile> flow_file) {
  if (!session_) {
    throw std::runtime_error("Access of ProcessSession after it has been released");
  }

  std::shared_ptr<script::ScriptFlowFile> result;

  if (flow_file == nullptr) {
    result = std::make_shared<script::ScriptFlowFile>(session_->create());
  } else {
    result = std::make_shared<script::ScriptFlowFile>(session_->create(flow_file->getFlowFile()));
  }

  flow_files_.push_back(result);
  return result;
}

void PyProcessSession::releaseCoreResources() {
  for (const auto &flow_file : flow_files_) {
    if (flow_file) {
      flow_file->releaseFlowFile();
    }
  }

  session_.reset();
}

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

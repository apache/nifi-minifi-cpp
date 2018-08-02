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

#ifndef NIFI_MINIFI_CPP_PYPROCESSESSION_H
#define NIFI_MINIFI_CPP_PYPROCESSESSION_H

#include <pybind11/embed.h>

#include <core/ProcessSession.h>

#include "../ScriptFlowFile.h"

#include "PyBaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace python {

namespace py = pybind11;

class PyProcessSession {
 public:
  explicit PyProcessSession(std::shared_ptr<core::ProcessSession> session);

  std::shared_ptr<script::ScriptFlowFile> get();
  std::shared_ptr<script::ScriptFlowFile> create();
  std::shared_ptr<script::ScriptFlowFile> create(std::shared_ptr<script::ScriptFlowFile> flow_file);
  void transfer(std::shared_ptr<script::ScriptFlowFile> flow_file, core::Relationship relationship);
  void read(std::shared_ptr<script::ScriptFlowFile> flow_file, py::object input_stream_callback);
  void write(std::shared_ptr<script::ScriptFlowFile> flow_file, py::object output_stream_callback);

  /**
   * Sometimes we want to release shared pointers to core resources when
   * we know they are no longer in need. This method is for those times.
   *
   * For example, we do not want to hold on to shared pointers to FlowFiles
   * after an onTrigger call, because doing so can be very expensive in terms
   * of repository resources.
   */
  void releaseCoreResources();

  class PyInputStreamCallback : public InputStreamCallback {
   public:
    explicit PyInputStreamCallback(const py::object &input_stream_callback) {
      py_callback_ = input_stream_callback;
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) override {
      auto py_stream = std::make_shared<PyBaseStream>(stream);
      return py_callback_.attr("process")(py_stream).cast<int64_t>();
    }

   private:
    py::object py_callback_;
  };

  class PyOutputStreamCallback : public OutputStreamCallback {
   public:
    explicit PyOutputStreamCallback(const py::object &output_stream_callback) {
      py_callback_ = output_stream_callback;
    }

    int64_t process(std::shared_ptr<io::BaseStream> stream) override {
      auto py_stream = std::make_shared<PyBaseStream>(stream);
      return py_callback_.attr("process")(py_stream).cast<int64_t>();
    }

   private:
    py::object py_callback_;
  };

 private:
  std::vector<std::shared_ptr<script::ScriptFlowFile>> flow_files_;
  std::shared_ptr<core::ProcessSession> session_;
};

} /* namespace python */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PYPROCESSESSION_H

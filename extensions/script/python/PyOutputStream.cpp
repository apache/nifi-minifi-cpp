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
#include <utility>
#include <string>

#include "PyOutputStream.h"

#include "utils/gsl.h"

namespace org::apache::nifi::minifi::python {

PyOutputStream::PyOutputStream(std::shared_ptr<io::OutputStream> stream)
    : stream_(std::move(stream)) {
}

size_t PyOutputStream::write(const py::bytes& buf) {
  return stream_->write(gsl::make_span(static_cast<std::string>(buf)).as_span<const std::byte>());
}

}  // namespace org::apache::nifi::minifi::python

/**
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
#include <optional>
#include "agent/MainHelper.h"
#include "core/extension/Extension.h"
#include "libwrapper/LibWrapper.h"
#include "sigslot/signal.hpp"

namespace org::apache::nifi::minifi::extensions::systemd {
struct SystemdLoader {
  std::unique_ptr<libwrapper::LibWrapper> libwrapper_ = libwrapper::createLibWrapper();
  sigslot::scoped_connection on_started_handler = service_started.connect_scoped([this] {
    libwrapper_->notify(false, "READY=1");
  });
  sigslot::scoped_connection on_stopping_handler = service_stopping.connect_scoped([this] {
    libwrapper_->notify(true, "STOPPING=1");
  });
};

namespace {
std::optional<SystemdLoader> loader;
bool init(const core::extension::ExtensionConfig&) try {
  loader.emplace();
  return true;
} catch(...) {
  return false;
}
void deinit() { loader.reset(); }
}  // namespace

REGISTER_EXTENSION("SystemdExtension", init, deinit);
}  // namespace org::apache::nifi::minifi::extensions::systemd

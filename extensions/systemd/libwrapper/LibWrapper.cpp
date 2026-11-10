// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "LibWrapper.h"
#include "DlopenWrapper.h"

namespace org::apache::nifi::minifi::extensions::systemd::libwrapper {

std::unique_ptr<LibWrapper> createLibWrapper() {
  return std::make_unique<DlopenWrapper>();
}

}  // namespace org::apache::nifi::minifi::extensions::systemd::libwrapper

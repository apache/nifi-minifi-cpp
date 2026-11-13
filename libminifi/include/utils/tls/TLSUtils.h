// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <atomic>
#include <cstdint>

namespace org::apache::nifi::minifi::utils::tls {

int pemPassWordCb(char *buf, int size, int rwflag, void *userdata);

}  // namespace org::apache::nifi::minifi::utils::tls

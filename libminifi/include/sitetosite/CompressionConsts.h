// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "io/InputStream.h"
#include "io/BufferStream.h"

namespace org::apache::nifi::minifi::sitetosite {

inline constexpr size_t COMPRESSION_BUFFER_SIZE = 65536;
inline constexpr std::array<char, 4> SYNC_BYTES = { 'S', 'Y', 'N', 'C' };

}  // namespace org::apache::nifi::minifi::sitetosite

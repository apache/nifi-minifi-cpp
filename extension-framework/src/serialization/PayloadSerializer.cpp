// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "serialization/PayloadSerializer.h"
#include "io/StreamPipe.h"

namespace org::apache::nifi::minifi {

io::IoResult PayloadSerializer::serialize(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<io::OutputStream>& out) {
  return reader_(flowFile, InputStreamPipe{*out});
}

}  // namespace org::apache::nifi::minifi

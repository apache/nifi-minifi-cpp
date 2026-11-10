// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include "io/InputStream.h"
#include "io/OutputStream.h"
#include "minifi-cpp/io/BaseStream.h"

namespace org::apache::nifi::minifi::io {

/**
 * Base Stream is the base of a composable stream architecture.
 * Intended to be the base of layered streams ala DatInputStreams in Java.
 *
 * ** Not intended to be thread safe as it is not intended to be shared**
 *
 * Extensions may be thread safe and thus shareable, but that is up to the implementation.
 */
class BaseStreamImpl : public virtual InputStreamImpl, public virtual OutputStreamImpl, public virtual BaseStream {};

}  // namespace org::apache::nifi::minifi::io

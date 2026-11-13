// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#ifndef EXTENSIONS_LIBARCHIVE_ARCHIVECOMMON_H_
#define EXTENSIONS_LIBARCHIVE_ARCHIVECOMMON_H_

#include <stdint.h>

#ifdef WIN32
typedef uint16_t mode_t;
typedef int64_t uid_t;
typedef int64_t gid_t;
#endif

#endif  // EXTENSIONS_LIBARCHIVE_ARCHIVECOMMON_H_

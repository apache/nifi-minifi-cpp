// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_PORT_PORT_H_
#define STORAGE_LEVELDB_PORT_PORT_H_

#include <string.h>

#ifdef WIN32
#include "port/port_win.h"
#else
#include "port/port_posix.h"
#endif

#endif  // STORAGE_LEVELDB_PORT_PORT_H_

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once
#include <string>
#include <system_error>
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <WinSock2.h>
#else
#include <sys/socket.h>
#endif /* WIN32 */

namespace org::apache::nifi::minifi::utils::net {

std::string sockaddr_ntop(const sockaddr* sa);

}  // namespace org::apache::nifi::minifi::utils::net

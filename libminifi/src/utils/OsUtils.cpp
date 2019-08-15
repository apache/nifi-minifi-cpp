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

#include "utils/OsUtils.h"
#include <iostream>
#include <map>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <sddl.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <mutex>
#include <pwd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#endif
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#ifdef WIN32
/*
 These are common translations for SIDs in windows
 */
std::string OsUtils::resolve_common_identifiers(const std::string &id) {
  static std::map<std::string, std::string> nameMap;
  if (nameMap.empty()) {
    nameMap["S-1-0"] = "Null Authority";
    nameMap["S-1-0-0"] = "Nobody";
    nameMap["S-1-1-0"] = "Everyone";
    nameMap["S-1-2"] = "Local Authority";
    nameMap["S-1-2-0"] = "Local";
    nameMap["S-1-2-1"] = "Console Logon";
    nameMap["S-1-3-0"] = "Creator Owner";
    nameMap["S-1-3-1"] = "Creator Group";
  }
  auto name = nameMap.find(id);
  if (name != std::end(nameMap)) {
    return name->second;
  }
  return "";
}
#endif

std::string OsUtils::userIdToUsername(const std::string &uid) {
  std::string name;
  name = uid;
  if (!name.empty()) {
#ifdef WIN32
    const auto resolved_name = resolve_common_identifiers(name);
    if (!resolved_name.empty()) {
      return resolved_name;
    }
    // First call to LookupAccountSid to get the buffer sizes.
    PSID pSidOwner = NULL;
    if (ConvertStringSidToSidA(name.c_str(), &pSidOwner)) {
      SID_NAME_USE sidType = SidTypeUnknown;
      DWORD windowsAccountNameSize = 0, dwwindowsDomainSize = 0;
      /*
       We can use a unique ptr with a deleter here but some of the calls
       below require we use global alloc -- so a global deleter to call GlobalFree
       won't buy us a ton unless we anticipate requiring more of this. If we do
       I suggest we break this out until a subset of OsUtils into our own convenience functions.
       */
      LPTSTR windowsDomain = NULL;
      LPTSTR windowsAccount = NULL;

      /*
       The first call will be to obtain sizes for domain and account,
       after which we will allocate the memory and free it after.
       In some cases youc an replace GlobalAlloc with
       */
      LookupAccountSid(NULL /** local computer **/, pSidOwner,
          windowsAccount,
          (LPDWORD)&windowsAccountNameSize,
          windowsDomain,
          (LPDWORD)&dwwindowsDomainSize,
          &sidType);

      if (windowsAccountNameSize > 0) {
        windowsAccount = (LPTSTR)GlobalAlloc(
            GMEM_FIXED,
            windowsAccountNameSize);

        if (dwwindowsDomainSize > 0) {
          windowsDomain = (LPTSTR)GlobalAlloc(
              GMEM_FIXED,
              dwwindowsDomainSize);
        }

        if (LookupAccountSid(
                NULL,
                pSidOwner,
                windowsAccount,
                (LPDWORD)&windowsAccountNameSize,
                windowsDomain,
                (LPDWORD)&dwwindowsDomainSize,
                &sidType)) {
          if (dwwindowsDomainSize > 0) {
            std::string domain = std::string(windowsDomain);
            name = domain + "\\";
            name += std::string(windowsAccount);
          } else {
            name = std::string(windowsAccount);
          }
        }
        GlobalFree(windowsAccount);
        if (dwwindowsDomainSize > 0)
        GlobalFree(windowsDomain);
      }
    }
#else
    auto ptr = name.c_str();
    char *end = nullptr;  // it will be unused
    uid_t ret = std::strtol(ptr, &end, 10);
    if (ret > 0) {
      struct passwd pass;
      struct passwd *result;
      char localbuf[1024];
      if (!getpwuid_r(ret, &pass, localbuf, sizeof localbuf, &result)) {
        name = pass.pw_name;
      }
    }
#endif
  }
  return name;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


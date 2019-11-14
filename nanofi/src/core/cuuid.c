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

// This needs to be included first to let uuid.h sort out the system header collisions
#ifndef WIN32
#include "uuid.h"
#endif

#include "core/cuuid.h"

#ifdef WIN32
#include "Rpc.h"
#include "Winsock2.h"
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ws2_32.lib")
#else
#include <pthread.h>
#endif

#ifdef WIN32
  void windows_uuid_to_str(const UUID* uuid, char* out) {
    RPC_CSTR str = NULL;
    UuidToStringA(uuid, &str);
    snprintf(out, 37, "%.36s", (char*)str);
    RpcStringFreeA(&str);
  }

  void windows_uuid_generate_time(char* out) {
    UUID uuid;
    UuidCreateSequential(&uuid);
    windows_uuid_to_str(&uuid, out);
  }

  void windows_uuid_generate_random(char* out) {
    UUID uuid;
    UuidCreate(&uuid);
    windows_uuid_to_str(&uuid, out);
  }
#else
  int generate_uuid_with_uuid_impl(unsigned int mode, char* out) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static uuid_t* uuid_impl = NULL;
    pthread_mutex_lock(&mutex);
    if (uuid_impl == NULL) {
      if (uuid_create(&uuid_impl) != UUID_RC_OK) {
        pthread_mutex_unlock(&mutex);
        return -1;
      }
    }
    uuid_make(uuid_impl, mode);
    size_t len = UUID_LEN_STR+1;
    if (uuid_export(uuid_impl, UUID_FMT_STR, &out, &len) != UUID_RC_OK) {
      pthread_mutex_unlock(&mutex);
      return -1;
    }
    pthread_mutex_unlock(&mutex);
    return 0;
  }
#endif

void generate_uuid(const CIDGenerator * generator, char * out) {
  switch (generator->implementation_) {
    case CUUID_RANDOM_IMPL:
    case CUUID_DEFAULT_IMPL:
#ifdef WIN32
      windows_uuid_generate_random(out);
#else
      generate_uuid_with_uuid_impl(UUID_MAKE_V4, out);
#endif
      break;
    case CUUID_TIME_IMPL:
    default:
#ifdef WIN32
      windows_uuid_generate_time(out);
#else
      generate_uuid_with_uuid_impl(UUID_MAKE_V1, out);
#endif
      break;
  }
}

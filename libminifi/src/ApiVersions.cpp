/**
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

#include "minifi-c/minifi-c.h"
#include "minifi-cpp/utils/Export.h"

extern "C" {

const MinifiApiVersion* MINIFI_API_VERSION_FN() {
  return reinterpret_cast<const MinifiApiVersion*>(MINIFI_PRIVATE_STRINGIFY(MINIFI_API_VERSION_FN));
}

#define REGISTER_C_API_VERSION(major, minor)  \
    const MinifiApiVersion* MINIFI_PRIVATE_JOIN(MinifiCApiVersion, MINIFI_PRIVATE_JOIN(major, minor))() {  \
      return reinterpret_cast<const MinifiApiVersion*>(MINIFI_PRIVATE_STRINGIFY(MINIFI_C_API_MAJOR_VERSION) "." MINIFI_PRIVATE_STRINGIFY(MINIFI_C_API_MINOR_VERSION) "." MINIFI_PRIVATE_STRINGIFY(MINIFI_C_API_PATCH_VERSION));  \
    }

REGISTER_C_API_VERSION(0, 1)

}  // extern "C"

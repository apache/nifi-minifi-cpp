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

#include "core/cuuid.h"

void generate_uuid(const CIDGenerator * generator, char * out) {
  UUID_FIELD output;
  switch (generator->implementation_) {
    case CUUID_RANDOM_IMPL:
      uuid_generate_random(output);
      break;
    case CUUID_DEFAULT_IMPL:
      uuid_generate(output);
      break;
    default:
      uuid_generate_time(output);
      break;
  }
  uuid_unparse_lower(output, out);
}

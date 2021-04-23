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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "api/nanofi.h"
#include "blocks/file_blocks.h"
#include "blocks/comms.h"
#include "core/processors.h"
#include "HTTPCurlLoader.h"
#include "python_lib.h"



#ifdef __cplusplus
extern "C" {
#endif


int init_api(const char *resource) {
  core::ClassLoader::getDefaultClassLoader().registerResource(resource, "createHttpCurlFactory");
  return 0;
}

#ifdef __cplusplus
}
#endif

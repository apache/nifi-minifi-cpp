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
#ifndef BLOCKS_FILE_BLOCKS_H_
#define BLOCKS_FILE_BLOCKS_H_

#include "../api/nanofi.h"
#include "core/processors.h"

#define KEEP_SOURCE 0x01
#define RECURSE 0x02

/**
 * Monitor directory can be combined into a current flow. to create an execution plan
 */
flow *monitor_directory(nifi_instance *instance, char *directory, flow *parent_flow, char flags) {
  GetFileConfig config;
  config.directory = directory;
  config.keep_source = flags & KEEP_SOURCE;
  config.recurse = flags & RECURSE;
  return create_getfile(instance, parent_flow, &config);
}

#endif /* BLOCKS_FILE_BLOCKS_H_ */

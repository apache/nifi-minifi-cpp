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

#ifndef NANOFI_INCLUDE_CORE_FLOWFILES_H_
#define NANOFI_INCLUDE_CORE_FLOWFILES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cstructs.h"
#include "api/ecu.h"
#include "sitetosite/CPeer.h"
#include "sitetosite/CRawSocketProtocol.h"

flow_file_list * add_flow_file_record(flow_file_list ** ff_list, flow_file_record * record);

void free_flow_file_list(flow_file_list ** ff_list);

void add_attributes(flow_file_record * ffr, const char * file_path, uint64_t curr_offset);

void update_attributes(flow_file_record * ffr, const char * file_path, uint64_t curr_offset);

void transmit_flow_files(nifi_instance * instance, flow_file_list * ff_list, int complete);

void transmit_payload(struct CRawSiteToSiteClient * client, struct flow_file_list * ff_list, int complete);

uint64_t flow_files_size(flow_file_list * ff_list);

void read_payload_and_transmit(struct flow_file_list * ffl, struct CRawSiteToSiteClient * client);

#ifdef __cplusplus
}
#endif

#endif /* NANOFI_INCLUDE_CORE_FLOWFILES_H_ */

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
#ifndef BLOCKS_COMMS_H_
#define BLOCKS_COMMS_H_

#include <stdio.h>

#include "../api/nanofi.h"
#include "core/processors.h"

#define SUCCESS 0x00
#define FINISHED_EARLY 0x01
#define FAIL 0x02

typedef int transmission_stop(void *);

uint8_t transmit_to_nifi(nifi_instance *instance, flow *flow, transmission_stop *stop_callback) {

  flow_file_record *record = 0x00;
  do {
    record = get_next_flow_file(instance, flow);

    if (record == 0) {
      return FINISHED_EARLY;
    }
    transmit_flowfile(record, instance);

    free_flowfile(record);
  } while (record != 0x00 && !(stop_callback != 0x00 && stop_callback(0x00)));
  return SUCCESS;
}

#endif /* BLOCKS_COMMS_H_ */

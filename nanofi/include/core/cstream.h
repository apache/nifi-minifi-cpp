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


#ifndef NIFI_MINIFI_CPP_CSTREAM_H
#define NIFI_MINIFI_CPP_CSTREAM_H

#include "cstructs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Bool {True = 1, False = 0};

int write_uint64_t(uint64_t value, cstream * stream);

int write_uint32_t(uint32_t value, cstream * stream);

int write_uint16_t(uint16_t value, cstream * stream);

int write_uint8_t(uint8_t value, cstream * stream);

int write_char(char value, cstream * stream);

int write_buffer(const uint8_t *value, int len, cstream * stream);

int writeUTF(const char * cstr, uint64_t len, enum Bool widen, cstream * stream);

int read_char(char *value, cstream * stream);

int read_uint8_t(uint8_t *value, cstream * stream);

int read_uint16_t(uint16_t *value, cstream * stream);

int read_uint32_t(uint32_t *value, cstream * stream);

int read_uint64_t(uint64_t *value, cstream * stream);

int read_buffer(uint8_t *value, int len, cstream * stream);

int readUTFLen(uint32_t * utflen, cstream * stream);

int readUTF(char * buf, uint64_t buflen, cstream * stream);

void close_stream(cstream * stream);

int open_stream(cstream * stream);

cstream * create_socket(const char * host, uint16_t portnum);

void free_socket(cstream * stream);

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_CSTREAM_H

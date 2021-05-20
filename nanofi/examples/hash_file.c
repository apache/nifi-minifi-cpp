/*
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

#ifdef OPENSSL_SUPPORT

#include "api/nanofi.h"
#include <openssl/md5.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void custom_processor_logic(processor_session * ps, processor_context * ctx) {
  flow_file_record * ffr = get(ps, ctx);
  if (ffr == NULL) {
    return;
  }
  uint8_t * buffer = (uint8_t*)malloc(ffr->size* sizeof(uint8_t));

  get_content(ffr, buffer, ffr->size);


  MD5_CTX context;
  MD5_Init(&context);
  MD5_Update(&context, buffer, ffr->size);

  free(buffer);

  unsigned char digest[MD5_DIGEST_LENGTH];
  MD5_Final(digest, &context);

  char md5string[33];
  for (int i = 0; i < 16; ++i) {
    sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
  }

  char prop_value[50];

  if (get_property(ctx, "checksum_attr_name", prop_value, 50) != 0) {
    return;  // Attr name not found
  }

  add_attribute(ffr, prop_value, (void*)md5string, strlen(md5string));

  transfer_to_relationship(ffr, ps, SUCCESS_RELATIONSHIP);

  free_flowfile(ffr);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Error: must run ./hash_file <file>\n");
    exit(1);
  }

  char *file = argv[1];

  if (access(file, F_OK) == -1) {
    printf("Error: %s doesn't exist!\n", file);
    exit(1);
  }

  struct stat stats;
  stat(file, &stats);

  // Check for file existence
  if (S_ISDIR(stats.st_mode)) {
    printf("Error: %s is a directory!\n", file);
    exit(1);
  }

  add_custom_processor("md5proc", custom_processor_logic, NULL);

  standalone_processor *standalone_proc = create_processor("md5proc");

  const char * checksum_attr_name = "md5attr";

  set_standalone_property(standalone_proc, "checksum_attr_name", checksum_attr_name);


  flow_file_record * ffr = create_flowfile(file, strlen(file));

  flow_file_record * new_ff = invoke_ff(standalone_proc, ffr);

  free_flowfile(ffr);

  attribute attr;
  attr.key = checksum_attr_name;
  attr.value_size = 0;
  get_attribute(new_ff, &attr);

  char md5value[50];

  snprintf(md5value, attr.value_size, "%s", (const char*)attr.value+1);

  printf("Checksum of %s is %s\n", file, md5value);

  free_flowfile(new_ff);

  free_standalone_processor(standalone_proc);

  return 0;
}

#endif  // OPENSSL_SUPPORT

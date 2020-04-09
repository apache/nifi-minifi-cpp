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

#ifndef NANOFI_INCLUDE_CORE_CORE_UTILS_H_
#define NANOFI_INCLUDE_CORE_CORE_UTILS_H_

#include <core/cstructs.h>

int add_property(struct properties ** head, const char * name, const char * value);
properties_t * clone_properties(properties_t * props);
void free_property(properties_t * prop);
void free_properties(properties_t * prop);
void serialize_properties(properties_t * props, char ** data, size_t * len);
attribute_set prepare_attributes(properties_t * attributes);
attribute_set unpack_metadata(char * meta, size_t len);

void free_attributes(attribute_set as);
attribute_set copy_attributes(attribute_set as);
attribute * find_attribute(attribute_set as, const char * key);
#endif /* NANOFI_INCLUDE_CORE_CORE_UTILS_H_ */

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

#ifndef NANOFI_INCLUDE_CORE_FILE_UTILS_H_
#define NANOFI_INCLUDE_CORE_FILE_UTILS_H_

#include "utlist.h"
#include "flowfiles.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Recursively deletes a directory tree
 * @param path, the path to the directory
 */
void remove_directory(const char * path);

/**
 * Determine if the provided directory/file path is a directory
 * @path the absolute path to the file/directory
 * @return 1 if path is directory else 0
 */
int is_directory(const char * path);

/*
 * Get the platform-specific path separator.
 * @param force_posix returns the posix path separator ('/'), even when not on posix. Useful when dealing with remote posix paths.
 * @return the path separator character
 */
const char * get_separator(int force_posix);

/**
 * Joins parent path with child path
 * @param parent the parent path
 * @param child the child path
 * @return concatenated path
 * @attention this function allocates memory for the returned concatenated path
 * and it is left for the caller to free the memory
 */
char * concat_path(const char * parent, const char * child);

/**
 * Make a directory tree specified by path
 * @param path the path to the directory
 * @return 1 if successful else 0
 */
int make_dir(const char * path);

/**
 * Return the current working directory
 * @return the current working directory
 * @attention this function allocates memory on heap
 * it is left to the caller to free it
 */
char * get_current_working_directory();

#ifdef __cplusplus
}
#endif

#endif /* NANOFI_INCLUDE_CORE_FILE_UTILS_H_ */

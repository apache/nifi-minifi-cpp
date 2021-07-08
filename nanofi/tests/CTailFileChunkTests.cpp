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
#ifndef _WIN32
#include "catch.hpp"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "core/string_utils.h"
#include "core/file_utils.h"

#include "CTestsBase.h"

/****
 * ##################################################################
 *  CTAILFILE CHUNK TESTS
 * ##################################################################
 */

TEST_CASE("Test tailfile chunk size 4096, file size 8KB", "[tailfileChunk8KBFileSize]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "4096";

    // Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.WriteNChars(4096, 'b');
    fm.CloseStream();

    standalone_processor * proc = mgr.getProcessor();
    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    flow_file_record* new_ff = invoke(proc);
    free(new_ff);

    char uuid_str[37];
    get_proc_uuid_from_processor(proc, uuid_str);
    struct processor_params * pp = get_proc_params(uuid_str);

    // Test that two flow file records were created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    // Test that the current offset in the file is 8192 bytes
    REQUIRE(pp->curr_offset == 8192);
}

TEST_CASE("Test tailfile chunk size 4096, file size less than 8KB", "[tailfileChunkFileSizeLessThan8KB]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "4096";

    // Write 4505 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.WriteNChars(409, 'b');
    fm.CloseStream();

    standalone_processor * proc = mgr.getProcessor();
    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    flow_file_record* new_ff = invoke(proc);
    free(new_ff);

    char uuid_str[37];
    get_proc_uuid_from_processor(proc, uuid_str);
    struct processor_params * pp = get_proc_params(uuid_str);
    // Test that one flow file record was created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);

    // Test that the current offset in the file is 4096 bytes
    REQUIRE(pp->curr_offset == 4096);
    REQUIRE(pp->ff_list->ff_record->size == 4096);

    struct stat fstat;
    REQUIRE(stat(pp->ff_list->ff_record->contentLocation, &fstat) == 0);
    REQUIRE(fstat.st_size == 4096);
}

TEST_CASE("Test tailfile chunk size 512, file size equal to 4608B", "[tailfileChunkFileSize8KB]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "512";

    // Write 4608 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4608, 'a');
    fm.CloseStream();

    standalone_processor * proc = mgr.getProcessor();
    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    flow_file_record* new_ff = invoke(proc);
    free(new_ff);

    char uuid_str[37];
    get_proc_uuid_from_processor(proc, uuid_str);
    struct processor_params * pp = get_proc_params(uuid_str);

    // Test that one flow file record was created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 9);

    // Test that the current offset in the file is 4608 bytes
    REQUIRE(pp->curr_offset == 4608);
}
#endif

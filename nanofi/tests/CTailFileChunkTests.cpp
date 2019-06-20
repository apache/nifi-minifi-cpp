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

    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "4096";

    //Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.WriteNChars(4096, 'b');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that two flow file records were created
    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 2);


    //Test that the current offset in the file is 8192 bytes
    REQUIRE(curr_offset == 8192);
}

TEST_CASE("Test tailfile chunk size 4096, file size less than 8KB", "[tailfileChunkFileSizeLessThan8KB]") {

    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "4096";

    //Write 4505 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.WriteNChars(409, 'b');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    ff_list = NULL;
    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that one flow file record was created
    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 1);

    //Test that the current offset in the file is 4096 bytes
    REQUIRE(curr_offset == 4096);
    REQUIRE(ff_list->ff_record->size == 4096);
    REQUIRE(file_size(ff_list->ff_record->contentLocation) == 4096);
}

TEST_CASE("Test tailfile chunk size 512, file size equal to 4608B", "[tailfileChunkFileSize8KB]") {

    TailFileTestResourceManager mgr("TailFileChunk", on_trigger_tailfilechunk);
    const char * file = "./e.txt";
    const char * chunksize = "512";

    //Write 4608 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4608, 'a');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "chunk_size", chunksize);

    ff_list = NULL;
    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that one flow file record was created
    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 9);

    //Test that the current offset in the file is 4608 bytes
    REQUIRE(curr_offset == 4608);
}
#endif

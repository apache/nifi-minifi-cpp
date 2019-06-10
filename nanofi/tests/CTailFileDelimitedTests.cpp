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

#include "catch.hpp"

#include "CTestsBase.h"

/****
 * ##################################################################
 *  CTAILFILE DELIMITED TESTS
 * ##################################################################
 */

TEST_CASE("Test tailfile delimited. Empty file", "[tailfileDelimitedEmptyFileTest]") {

    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";
    const char * delimiter = ";";

    //Create empty file
    FileManager fm(file);

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", ";");

    curr_offset = 0;
    ff_list = NULL;
    flow_file_record * new_ff = invoke(proc);

    //Test that no flowfiles were created
    REQUIRE(ff_list == NULL);
    REQUIRE(flow_files_size(ff_list) == 0);

    //Test that the current offset in the file is 42 bytes
    REQUIRE(curr_offset == 0);
}

TEST_CASE("Test tailfile delimited. File has less than 4096 chars", "[tailfileDelimitedLessThan4096Chars]") {

    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";
    const char * delimiter = ";";

    //Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", ";");

    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //No flow files will be created
    REQUIRE(ff_list == NULL);
    REQUIRE(flow_files_size(ff_list) == 0);

    //Test that the current offset in the file is 0
    REQUIRE(curr_offset == 0);
}

TEST_CASE("Test tailfile delimited. Simple test", "[tailfileDelimitedSimpleTest]") {

    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";
    const char * delimiter = ";";

    //Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.WriteNChars(1, ';');
    fm.WriteNChars(6, 'b');
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", ";");

    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that two flow file records were created
    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 2);

    //Test that the current offset in the file is 42 bytes
    REQUIRE(curr_offset == 42);

    //Test the flow file sizes
    const char * flowfile1_path = ff_list->ff_record->contentLocation;
    const char * flowfile2_path = ff_list->next->ff_record->contentLocation;

    struct stat fstat;
    stat(flowfile1_path, &fstat);
    REQUIRE(fstat.st_size == 34);

    stat(flowfile2_path, &fstat);
    REQUIRE(fstat.st_size == 6);
}

TEST_CASE("Test tailfile delimited. trailing non delimited string", "[tailfileNonDelimitedTest]") {

    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";
    const char * delimiter = ";";

    //Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.WriteNChars(1, ';');
    fm.WriteNChars(32, 'b');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", ";");

    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that two flow file records were created
    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 1);

    //Test that the current offset in the file is 35 bytes
    REQUIRE(curr_offset == 35);

    struct stat fstat;
    stat(ff_list->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 34);

    //Append a delimiter at the end of the file
    fm.OpenStream();
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    invoke(proc);

    REQUIRE(flow_files_size(ff_list) == 2);

    stat(ff_list->next->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 32);
}

TEST_CASE("Test tailfile delimited 4096 chars non delimited", "[tailfileDelimitedSimpleTest]") {

    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";
    const char * delimiter = ";";

    //Write 4096 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.CloseStream();

    set_standalone_property(proc, "file_path", file);
    set_standalone_property(proc, "delimiter", ";");

    curr_offset = 0;
    flow_file_record * new_ff = invoke(proc);

    //Test that two flow file records were created
    REQUIRE(ff_list == NULL);

    //Test that the current offset in the file is 4096 bytes
    REQUIRE(curr_offset == 4096);

    //Write another 2048 characters
    fm.OpenStream();
    fm.WriteNChars(2048, 'b');
    fm.CloseStream();

    invoke(proc);

    REQUIRE(ff_list == NULL);
    REQUIRE(flow_files_size(ff_list) == 0);

    //Test that the current offset in the file is 4096 bytes only
    REQUIRE(curr_offset == 4096);

    //Write another 2048 characters
    fm.OpenStream();
    fm.WriteNChars(2048, 'c');
    fm.CloseStream();

    invoke(proc);

    REQUIRE(ff_list == NULL);
    REQUIRE(flow_files_size(ff_list) == 0);

    //Test that the current offset in the file is 8192 bytes only
    REQUIRE(curr_offset == 8192);

    //Write a delimiter at the end and expect a flow file size of 8192 bytes
    fm.OpenStream();
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    invoke(proc);

    REQUIRE(ff_list != NULL);
    REQUIRE(ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(ff_list) == 1);
    const char * flowfile_path = ff_list->ff_record->contentLocation;
    struct stat fstat;
    stat(flowfile_path, &fstat);
    REQUIRE(fstat.st_size == 8192);
}

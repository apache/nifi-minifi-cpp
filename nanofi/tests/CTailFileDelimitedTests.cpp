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
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    // Create empty file
    FileManager fm(file);

    auto pp = invoke_processor(mgr, file);

    // Test that no flowfiles were created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list == NULL);
}

TEST_CASE("Test tailfile delimited. File has less than 4096 chars", "[tailfileDelimitedLessThan4096Chars]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.CloseStream();

    auto pp = invoke_processor(mgr, file);

    // No flow files will be created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);
    REQUIRE(pp->ff_list->complete == 0);

    // Test that the current offset in the file is 34
    REQUIRE(pp->curr_offset == 34);
}

TEST_CASE("Test tailfile delimited. Simple test", "[tailfileDelimitedSimpleTest]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    // Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.WriteNChars(1, ';');
    fm.WriteNChars(6, 'b');
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    auto pp = invoke_processor(mgr, file);

    // Test that two flow file records were created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    // Test that the current offset in the file is 42 bytes
    REQUIRE(pp->curr_offset == 42);

    // Test the flow file sizes
    const char * flowfile1_path = pp->ff_list->ff_record->contentLocation;
    const char * flowfile2_path = pp->ff_list->next->ff_record->contentLocation;

    struct stat fstat;
    stat(flowfile1_path, &fstat);
    REQUIRE(fstat.st_size == 34);

    stat(flowfile2_path, &fstat);
    REQUIRE(fstat.st_size == 6);

    REQUIRE(pp->ff_list->complete == 1);
    REQUIRE(pp->ff_list->next->complete == 1);
}

TEST_CASE("Test tailfile delimited. trailing non delimited string", "[tailfileNonDelimitedTest]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    // Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(34, 'a');
    fm.WriteNChars(1, ';');
    fm.WriteNChars(32, 'b');
    fm.CloseStream();

    auto pp = invoke_processor(mgr, file);

    // Test that two flow file records were created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    // Test that the current offset in the file is 35 bytes
    REQUIRE(pp->curr_offset == 67);
    REQUIRE(pp->ff_list->complete == 1);
    REQUIRE(pp->ff_list->next->complete == 0);
    struct stat fstat;
    stat(pp->ff_list->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 34);

    // Append a delimiter at the end of the file
    fm.OpenStream();
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    pp = invoke_processor(mgr, file);
    REQUIRE(pp != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    stat(pp->ff_list->next->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 32);
    REQUIRE(pp->ff_list->next->complete == 1);
}

TEST_CASE("Test tailfile delimited 4096 chars non delimited", "[tailfileDelimitedSimpleTest]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    // Write 4096 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(4096, 'a');
    fm.CloseStream();

    auto pp = invoke_processor(mgr, file);

    REQUIRE(pp !=  NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);
    REQUIRE(pp->ff_list->complete == 0);
    // Test that the current offset in the file is 4096 bytes
    REQUIRE(pp->curr_offset == 4096);

    // Write another 2048 characters
    fm.OpenStream();
    fm.WriteNChars(2048, 'b');
    fm.CloseStream();

    pp = invoke_processor(mgr, file);

    REQUIRE(pp->ff_list != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);
    REQUIRE(pp->ff_list->complete == 0);

    // Test that the current offset in the file is (4096 + 2048)
    REQUIRE(pp->curr_offset == 6144);

    // Write another 2048 characters
    fm.OpenStream();
    fm.WriteNChars(2048, 'c');
    fm.CloseStream();

    pp = invoke_processor(mgr, file);

    REQUIRE(pp->ff_list != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);

    // Test that the current offset in the file is 8192 bytes only
    REQUIRE(pp->curr_offset == 8192);

    // Write a delimiter at the end and expect a flow file size of 8192 bytes
    fm.OpenStream();
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    pp = invoke_processor(mgr, file);

    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 1);
    REQUIRE(pp->ff_list->complete == 1);
    const char * flowfile_path = pp->ff_list->ff_record->contentLocation;
    struct stat fstat;
    stat(flowfile_path, &fstat);
    REQUIRE(fstat.st_size == 8192);
}

TEST_CASE("Test tailfile delimited. string starting with delimiter", "[tailfileDelimiterStartStringTest]") {
    TestControllerWithTemporaryWorkingDirectory test_controller;
    TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
    const char * file = "./e.txt";

    // Write 8192 bytes to the file
    FileManager fm(file);
    fm.WriteNChars(5, ';');
    fm.WriteNChars(34, 'a');
    fm.WriteNChars(4, ';');
    fm.WriteNChars(32, 'b');
    fm.CloseStream();

    auto pp = invoke_processor(mgr, file);

    // Test that two flow file records were created
    REQUIRE(pp != NULL);
    REQUIRE(pp->ff_list != NULL);
    REQUIRE(pp->ff_list->ff_record != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    // Test that the current offset in the file is 35 bytes
    REQUIRE(pp->curr_offset == 75);
    REQUIRE(pp->ff_list->complete == 1);
    REQUIRE(pp->ff_list->next->complete == 0);
    struct stat fstat;
    stat(pp->ff_list->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 34);

    // Append a delimiter at the end of the file
    fm.OpenStream();
    fm.WriteNChars(1, ';');
    fm.CloseStream();

    pp = invoke_processor(mgr, file);
    REQUIRE(pp != NULL);
    REQUIRE(flow_files_size(pp->ff_list) == 2);

    stat(pp->ff_list->next->ff_record->contentLocation, &fstat);
    REQUIRE(fstat.st_size == 32);
    REQUIRE(pp->ff_list->next->complete == 1);
}

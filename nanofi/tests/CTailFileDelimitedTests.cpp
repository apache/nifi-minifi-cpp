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
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  //Create empty file
  FileManager fm(file_path);

  auto pp = invoke_processor(mgr, file_path);

  //Test that no flowfiles were created
  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list == NULL);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tailfile delimited. File has less than 4096 chars", "[tailfileDelimitedLessThan4096Chars]") {

  TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  FileManager fm(file_path);
  fm.WriteNChars(34, 'a');
  fm.CloseStream();

  auto pp = invoke_processor(mgr, file_path);

  //No flow files will be created
  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);
  REQUIRE(pp->ff_list->complete == 0);

  //Test that the current offset in the file is 34
  REQUIRE(pp->curr_offset == 34);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tailfile delimited. Simple test", "[tailfileDelimitedSimpleTest]") {

  TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  //Write 8192 bytes to the file
  FileManager fm(file_path);
  fm.WriteNChars(34, 'a');
  fm.WriteNChars(1, ';');
  fm.WriteNChars(6, 'b');
  fm.WriteNChars(1, ';');
  fm.CloseStream();

  auto pp = invoke_processor(mgr, file_path);

  //Test that two flow file records were created
  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(pp->ff_list->ff_record != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  //Test that the current offset in the file is 42 bytes
  REQUIRE(pp->curr_offset == 42);

  //Test the flow file sizes
  const char * flowfile1_path = pp->ff_list->ff_record->contentLocation;
  const char * flowfile2_path = pp->ff_list->next->ff_record->contentLocation;

  uint64_t fsize = 0;
  int ret = get_file_size(flowfile1_path, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 34);

  ret = get_file_size(flowfile2_path, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 6);

  REQUIRE(pp->ff_list->complete == 1);
  REQUIRE(pp->ff_list->next->complete == 1);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tailfile delimited. trailing non delimited string", "[tailfileNonDelimitedTest]") {

  TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  //Write 8192 bytes to the file
  FileManager fm(file_path);
  fm.WriteNChars(34, 'a');
  fm.WriteNChars(1, ';');
  fm.WriteNChars(32, 'b');
  fm.CloseStream();

  auto pp = invoke_processor(mgr, file_path);

  //Test that two flow file records were created
  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(pp->ff_list->ff_record != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  //Test that the current offset in the file is 35 bytes
  REQUIRE(pp->curr_offset == 67);
  REQUIRE(pp->ff_list->complete == 1);
  REQUIRE(pp->ff_list->next->complete == 0);
  uint64_t fsize = 0;
  int ret = get_file_size(pp->ff_list->ff_record->contentLocation, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 34);

  //Append a delimiter at the end of the file
  fm.OpenStream();
  fm.WriteNChars(1, ';');
  fm.CloseStream();

  pp = invoke_processor(mgr, file_path);
  REQUIRE(pp != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  ret = get_file_size(pp->ff_list->next->ff_record->contentLocation, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 32);
  REQUIRE(pp->ff_list->next->complete == 1);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tailfile delimited 4096 chars non delimited", "[tailfileDelimitedSimpleTest]") {

  TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  //Write 4096 bytes to the file
  FileManager fm(file_path);
  fm.WriteNChars(4096, 'a');
  fm.CloseStream();

  auto pp = invoke_processor(mgr, file_path);

  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);
  REQUIRE(pp->ff_list->complete == 0);
  //Test that the current offset in the file is 4096 bytes
  REQUIRE(pp->curr_offset == 4096);

  //Write another 2048 characters
  fm.OpenStream();
  fm.WriteNChars(2048, 'b');
  fm.CloseStream();

  pp = invoke_processor(mgr, file_path);

  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);
  REQUIRE(pp->ff_list->complete == 0);

  //Test that the current offset in the file is (4096 + 2048)
  REQUIRE(pp->curr_offset == 6144);

  //Write another 2048 characters
  fm.OpenStream();
  fm.WriteNChars(2048, 'c');
  fm.CloseStream();

  pp = invoke_processor(mgr, file_path);

  REQUIRE(pp->ff_list != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);

  //Test that the current offset in the file is 8192 bytes only
  REQUIRE(pp->curr_offset == 8192);

  //Write a delimiter at the end and expect a flow file size of 8192 bytes
  fm.OpenStream();
  fm.WriteNChars(1, ';');
  fm.CloseStream();

  pp = invoke_processor(mgr, file_path);

  REQUIRE(pp->ff_list != NULL);
  REQUIRE(pp->ff_list->ff_record != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 1);
  REQUIRE(pp->ff_list->complete == 1);
  uint64_t fsize = 0;
  int ret = get_file_size(pp->ff_list->ff_record->contentLocation, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 8192);

  remove_temp_directory(file_path);
  free(file_path);
}

TEST_CASE("Test tailfile delimited. string starting with delimiter", "[tailfileDelimiterStartStringTest]") {

  TailFileTestResourceManager mgr("TailFileDelimited", on_trigger_tailfiledelimited);
  const char * file = "e.txt";
  const char * delimiter = ";";

  char * file_path = get_temp_file_path(file);
  REQUIRE(file_path != NULL);

  //Write 8192 bytes to the file
  FileManager fm(file_path);
  fm.WriteNChars(5, ';');
  fm.WriteNChars(34, 'a');
  fm.WriteNChars(4, ';');
  fm.WriteNChars(32, 'b');
  fm.CloseStream();

  auto pp = invoke_processor(mgr, file_path);

  //Test that two flow file records were created
  REQUIRE(pp != NULL);
  REQUIRE(pp->ff_list != NULL);
  REQUIRE(pp->ff_list->ff_record != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  //Test that the current offset in the file is 35 bytes
  REQUIRE(pp->curr_offset == 75);
  REQUIRE(pp->ff_list->complete == 1);
  REQUIRE(pp->ff_list->next->complete == 0);
  uint64_t fsize = 0;
  int ret = get_file_size(pp->ff_list->ff_record->contentLocation, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 34);

  //Append a delimiter at the end of the file
  fm.OpenStream();
  fm.WriteNChars(1, ';');
  fm.CloseStream();

  pp = invoke_processor(mgr, file_path);
  REQUIRE(pp != NULL);
  REQUIRE(flow_files_size(pp->ff_list) == 2);

  ret = get_file_size(pp->ff_list->next->ff_record->contentLocation, &fsize);
  REQUIRE(ret == 0);
  REQUIRE(fsize == 32);
  REQUIRE(pp->ff_list->next->complete == 1);

  remove_temp_directory(file_path);
  free(file_path);
}

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
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>


#include "utils/file/FileUtils.h"
#include "../TestBase.h"

#include "capi/api.h"

#include <chrono>
#include <thread>

TEST_CASE("Test Creation of instance, one processor", "[createInstanceAndFlow]") {
  nifi_instance *instance = create_instance("random_instance", create_port("12345"));
  REQUIRE(instance != nullptr);
  flow *test_flow = create_flow(instance, nullptr);
  REQUIRE(test_flow != nullptr);
  processor *test_proc = add_processor(test_flow, "GenerateFlowFile");
  REQUIRE(test_proc != nullptr);
  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Invalid processor returns null", "[addInvalidProcessor]") {
  nifi_instance *instance = create_instance("random_instance", create_port("12345"));
  REQUIRE(instance != nullptr);
  flow *test_flow = create_flow(instance, nullptr);
  processor *test_proc = add_processor(test_flow, "NeverExisted");
  REQUIRE(test_proc == nullptr);
  processor *no_proc = add_processor(test_flow, "");
  REQUIRE(no_proc == nullptr);
  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Set valid and invalid properties", "[setProcesssorProperties]") {
  nifi_instance *instance = create_instance("random_instance", create_port("12345"));
  REQUIRE(instance != nullptr);
  flow *test_flow = create_flow(instance, nullptr);
  REQUIRE(test_flow != nullptr);
  processor *test_proc = add_processor(test_flow, "GenerateFlowFile");
  REQUIRE(test_proc != nullptr);
  REQUIRE(set_property(test_proc, "Data Format", "Text") == 0);  // Valid value
  // TODO(aboda): add this two below when property handling is made strictly typed
  // REQUIRE(set_property(test_proc, "Data Format", "InvalidFormat") != 0); // Invalid value
  // REQUIRE(set_property(test_proc, "Invalid Attribute", "Blah") != 0); // Invalid attribute
  REQUIRE(set_property(test_proc, "Data Format", nullptr) != 0);  // Empty value
  REQUIRE(set_property(test_proc, nullptr, "Blah") != 0);  // Empty attribute
  REQUIRE(set_property(nullptr, "Invalid Attribute", "Blah") != 0);  // Invalid processor
  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("get file and put file", "[getAndPutFile]") {
  TestController testController;

  char src_format[] = "/tmp/gt.XXXXXX";
  char put_format[] = "/tmp/pt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);
  const char *putfiledir = testController.createTempDirectory(put_format);
  std::string test_file_content = "C API raNdOMcaSe test d4t4 th1s is!";

  nifi_instance *instance = create_instance("random_instance", create_port("12345"));
  REQUIRE(instance != nullptr);
  flow *test_flow = create_flow(instance, nullptr);
  REQUIRE(test_flow != nullptr);
  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  processor *put_proc = add_processor_with_linkage(test_flow, "PutFile");
  REQUIRE(put_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);
  REQUIRE(set_property(put_proc, "Directory", putfiledir) == 0);

  std::fstream file;
  std::stringstream ss;
  ss << sourcedir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << test_file_content;
  file.close();

  flow_file_record *record = get_next_flow_file(instance, test_flow);
  REQUIRE(record != nullptr);

  ss.str("");

  ss << putfiledir << "/" << "tstFile.ext";
  std::ifstream t(ss.str());
  std::string put_data((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  REQUIRE(test_file_content == put_data);

  free_flowfile(record);

  free_flow(test_flow);

  free_instance(instance);
}

TEST_CASE("Test manipulation of attributes", "[testAttributes]") {
  TestController testController;

  enable_logging();

  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);

  std::string test_file_content = "C API raNdOMcaSe test d4t4 th1s is!";

  std::fstream file;
  std::stringstream ss;
  ss << sourcedir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << test_file_content;
  file.close();

  nifi_instance *instance = create_instance("random_instance", create_port("12345"));
  REQUIRE(instance != nullptr);
  flow *test_flow = create_flow(instance, nullptr);
  REQUIRE(test_flow != nullptr);

  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);
  processor *extract_test  = add_processor_with_linkage(test_flow, "ExtractText");
  REQUIRE(extract_test != nullptr);
  REQUIRE(set_property(extract_test, "Attribute", "TestAttr") == 0);
  // TODO(aboda): enable this after decision made in MINIFICPP-640
  /*processor *update_attribute = add_processor_with_linkage(test_flow, "UpdateAttribute");
  REQUIRE(update_attribute != nullptr);

  REQUIRE(set_property(update_attribute, "TestAttribute", "TestValue") == 0);*/

  flow_file_record *record = get_next_flow_file(instance, test_flow);

  REQUIRE(record != nullptr);

  attribute test_attr;
  test_attr.key = "TestAttr";
  REQUIRE(get_attribute(record, &test_attr) == 0);

  REQUIRE(test_attr.value_size != 0);
  REQUIRE(test_attr.value != nullptr);

  std::string attr_value(static_cast<char*>(test_attr.value), test_attr.value_size);

  REQUIRE(attr_value == test_file_content);

  const char * new_testattr_value = "S0me t3st t3xt";

  // Attribute already exist, should fail
  REQUIRE(add_attribute(record, test_attr.key, (void*)new_testattr_value, strlen(new_testattr_value)) != 0); // NOLINT

  // Update overwrites values
  update_attribute(record, test_attr.key, (void*)new_testattr_value, strlen(new_testattr_value)); // NOLINT

  int attr_size = get_attribute_qty(record);
  REQUIRE(attr_size > 0);

  attribute_set attr_set;
  attr_set.size = attr_size;
  attr_set.attributes = (attribute*)malloc(attr_set.size * sizeof(attribute)); // NOLINT

  REQUIRE(get_all_attributes(record, &attr_set) == attr_set.size);

  bool test_attr_found = false;
  for (int i = 0; i < attr_set.size; ++i) {
    if (strcmp(attr_set.attributes[i].key, test_attr.key) == 0) {
      test_attr_found = true;
      REQUIRE(std::string(static_cast<char*>(attr_set.attributes[i].value), attr_set.attributes[i].value_size) == new_testattr_value);
    }
  }
  REQUIRE(test_attr_found == true);

  free_flowfile(record);

  free_flow(test_flow);
  free_instance(instance);
}

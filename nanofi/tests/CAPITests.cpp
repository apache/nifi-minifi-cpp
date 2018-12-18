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
#include "TestBase.h"

#include <chrono>
#include <thread>
#include "api/nanofi.h"

std::string test_file_content = "C API raNdOMcaSe test d4t4 th1s is!";
std::string test_file_name = "tstFile.ext";

static nifi_instance *create_instance_obj(const char *name = "random_instance") {
  nifi_port port;
  char port_str[] = "12345";
  port.port_id = port_str;
  return create_instance("random_instance", &port);
}

static int failure_count = 0;

void failure_counter(flow_file_record * fr) {
  failure_count++;
  REQUIRE(get_attribute_quantity(fr) > 0);
  free_flowfile(fr);
}

void big_failure_counter(flow_file_record * fr) {
  failure_count += 100;
  free_flowfile(fr);
}

void custom_processor_logic(processor_session * ps, processor_context * ctx) {
  flow_file_record * ffr = get(ps, ctx);
  REQUIRE(ffr != nullptr);
  uint8_t * buffer = (uint8_t*)malloc(ffr->size* sizeof(uint8_t));
  get_content(ffr, buffer, ffr->size);
  REQUIRE(strncmp(reinterpret_cast<const char *>(buffer), test_file_content.c_str(), test_file_content.size()) == 0);

  attribute attr;
  attr.key = "filename";
  attr.value_size = 0;
  REQUIRE(get_attribute(ffr, &attr) == 0);
  REQUIRE(attr.value_size > 0);

  const char * custom_value = "custom value";

  REQUIRE(add_attribute(ffr, "custom attribute", (void*)custom_value, strlen(custom_value)) == 0);

  char prop_value[20];

  REQUIRE(get_property(ctx, "Some test propery", prop_value, 20) == 0);

  REQUIRE(strncmp("test value", prop_value, strlen(prop_value)) == 0);

  transfer_to_relationship(ffr, ps, SUCCESS_RELATIONSHIP);

  free_flowfile(ffr);
  free(buffer);
}

std::string create_testfile_for_getfile(const char* sourcedir, const std::string& filename = test_file_name) {
  std::fstream file;
  std::stringstream ss;
  ss << sourcedir << "/" << filename;
  file.open(ss.str(), std::ios::out);
  file << test_file_content;
  file.close();
  return ss.str();
}

TEST_CASE("Test Creation of instance, one processor", "[createInstanceAndFlow]") {
  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);
  processor *test_proc = add_processor(test_flow, "GenerateFlowFile");
  REQUIRE(test_proc != nullptr);
  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Invalid processor returns null", "[addInvalidProcessor]") {
  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  processor *test_proc = add_processor(test_flow, "NeverExisted");
  REQUIRE(test_proc == nullptr);
  processor *no_proc = add_processor(test_flow, "");
  REQUIRE(no_proc == nullptr);
  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Set valid and invalid properties", "[setProcesssorProperties]") {
  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
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
  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);
  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  processor *put_proc = add_processor(test_flow, "PutFile");
  REQUIRE(put_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);
  REQUIRE(set_property(put_proc, "Directory", putfiledir) == 0);

  create_testfile_for_getfile(sourcedir);

  flow_file_record *record = get_next_flow_file(instance, test_flow);
  REQUIRE(record != nullptr);

  std::stringstream ss;

  ss << putfiledir << "/" << test_file_name;
  std::ifstream t(ss.str());
  std::string put_data((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  REQUIRE(test_file_content == put_data);

  // No failure handler can be added after the flow is finalized
  REQUIRE(add_failure_callback(test_flow, failure_counter) == 1);

  uint8_t* content = (uint8_t*)malloc(record->size* sizeof(uint8_t));

  REQUIRE(get_content(record, content, record->size) == record->size);

  REQUIRE(test_file_content == std::string(reinterpret_cast<char*>(content), record->size));

  free(content);

  free_flowfile(record);

  free_flow(test_flow);

  free_instance(instance);
}

TEST_CASE("Test manipulation of attributes", "[testAttributes]") {
  TestController testController;

  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);

  create_testfile_for_getfile(sourcedir);

  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);

  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);
  processor *extract_test = add_processor(test_flow, "ExtractText");
  REQUIRE(extract_test != nullptr);
  REQUIRE(set_property(extract_test, "Attribute", "TestAttr") == 0);
  processor *update_attr = add_processor(test_flow, "UpdateAttribute");
  REQUIRE(update_attr != nullptr);

  REQUIRE(set_property(update_attr, "UpdatedAttribute", "UpdatedValue") == 0);

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
  REQUIRE(add_attribute(record, test_attr.key, (void*) new_testattr_value, strlen(new_testattr_value)) != 0);  // NOLINT

  // Update overwrites values
  update_attribute(record, test_attr.key, (void*) new_testattr_value, strlen(new_testattr_value));  // NOLINT

  int attr_size = get_attribute_quantity(record);
  REQUIRE(attr_size > 0);

  attribute_set attr_set;
  attr_set.size = attr_size;
  attr_set.attributes = (attribute*) malloc(attr_set.size * sizeof(attribute));  // NOLINT

  REQUIRE(get_all_attributes(record, &attr_set) == attr_set.size);

  bool test_attr_found = false;
  bool updated_attr_found = false;
  for (int i = 0; i < attr_set.size; ++i) {
    if (strcmp(attr_set.attributes[i].key, test_attr.key) == 0) {
      test_attr_found = true;
      REQUIRE(std::string(static_cast<char*>(attr_set.attributes[i].value), attr_set.attributes[i].value_size) == new_testattr_value);
    } else if (strcmp(attr_set.attributes[i].key, "UpdatedAttribute") == 0) {
      updated_attr_found = true;
      REQUIRE(std::string(static_cast<char*>(attr_set.attributes[i].value), attr_set.attributes[i].value_size) == "UpdatedValue");
    }
  }
  REQUIRE(updated_attr_found == true);
  REQUIRE(test_attr_found == true);

  free_flowfile(record);

  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Test error handling callback", "[errorHandling]") {
  TestController testController;
  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);

  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);

  // Failure strategy cannot be set before a valid callback is added
  REQUIRE(set_failure_strategy(test_flow, FailureStrategy::AS_IS) != 0);
  REQUIRE(add_failure_callback(test_flow, failure_counter) == 0);

  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  processor *put_proc = add_processor(test_flow, "PutFile");
  REQUIRE(put_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);
  REQUIRE(set_property(put_proc, "Directory", "/tmp/never_existed") == 0);
  REQUIRE(set_property(put_proc, "Create Missing Directories", "false") == 0);

  create_testfile_for_getfile(sourcedir);


  REQUIRE(get_next_flow_file(instance, test_flow) == nullptr);

  REQUIRE(failure_count == 1);

  // Failure handler function can be replaced runtime
  REQUIRE(add_failure_callback(test_flow, big_failure_counter) == 0);
  REQUIRE(set_failure_strategy(test_flow, FailureStrategy::ROLLBACK) == 0);

  // Create new testfile to trigger failure again
  create_testfile_for_getfile(sourcedir, test_file_name + "2");

  REQUIRE(get_next_flow_file(instance, test_flow) == nullptr);
  REQUIRE(failure_count > 100);

  failure_count = 0;

  free_flow(test_flow);
  free_instance(instance);
}

TEST_CASE("Test standalone processors", "[testStandalone]") {
  TestController testController;

  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);

  create_testfile_for_getfile(sourcedir);

  standalone_processor* getfile_proc = create_processor("GetFile");
  REQUIRE(set_standalone_property(getfile_proc, "Input Directory", sourcedir) == 0);

  flow_file_record* ffr = invoke(getfile_proc);

  REQUIRE(ffr != nullptr);
  REQUIRE(get_attribute_quantity(ffr) > 0);

  standalone_processor* extract_test = create_processor("ExtractText");
  REQUIRE(extract_test != nullptr);
  REQUIRE(set_standalone_property(extract_test, "Attribute", "TestAttr") == 0);

  flow_file_record* ffr2 = invoke_ff(extract_test, ffr);

  free_flowfile(ffr);

  // Verify the transfer of attributes
  REQUIRE(ffr2 != nullptr);
  REQUIRE(get_attribute_quantity(ffr2) > 0);

  char filename_key[] = "filename";
  attribute attr;
  attr.key = filename_key;
  attr.value_size = 0;

  REQUIRE(get_attribute(ffr2, &attr) == 0);
  REQUIRE(attr.value_size > 0);

  // Verify extracttext behavior
  char test_attr[] = "TestAttr";
  attr.key = test_attr;
  attr.value_size = 0;
  REQUIRE(get_attribute(ffr2, &attr) == 0);
  REQUIRE(std::string(static_cast<char*>(attr.value), attr.value_size) == test_file_content);

  free_flowfile(ffr2);
  free_standalone_processor(getfile_proc);
}

TEST_CASE("Test interaction of flow and standlone processors", "[testStandaloneWithFlow]") {
  TestController testController;

  char src_format[] = "/tmp/gt.XXXXXX";
  char put_format[] = "/tmp/pt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);
  const char *putfiledir = testController.createTempDirectory(put_format);

  create_testfile_for_getfile(sourcedir);

  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);

  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);
  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);

  flow_file_record *record = get_next_flow_file(instance, test_flow);
  REQUIRE(record != nullptr);

  standalone_processor* putfile_proc = create_processor("PutFile");
  REQUIRE(set_standalone_property(putfile_proc, "Directory", putfiledir) == 0);

  flow_file_record* put_record = invoke_ff(putfile_proc, record);
  REQUIRE(put_record != nullptr);

  free_flowfile(record);
  free_flowfile(put_record);

  std::stringstream ss;

  ss << putfiledir << "/" << test_file_name;
  std::ifstream t(ss.str());
  std::string put_data((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

  REQUIRE(test_file_content == put_data);

  free_flow(test_flow);
  free_instance(instance);
  free_standalone_processor(putfile_proc);
}

TEST_CASE("Test standalone processors with file input", "[testStandaloneWithFile]") {
  TestController testController;

  enable_logging();
  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);
  std::string path = create_testfile_for_getfile(sourcedir);

  standalone_processor* extract_test = create_processor("ExtractText");
  REQUIRE(extract_test != nullptr);
  REQUIRE(set_standalone_property(extract_test, "Attribute", "TestAttr") == 0);

  flow_file_record* ffr = invoke_file(extract_test, path.c_str());

  REQUIRE(ffr != nullptr);

  attribute attr;
  char test_attr[] = "TestAttr";
  attr.key = test_attr;
  attr.value_size = 0;
  REQUIRE(get_attribute(ffr, &attr) == 0);
  REQUIRE(std::string(static_cast<char*>(attr.value), attr.value_size) == test_file_content);

  free_flowfile(ffr);
  free_standalone_processor(extract_test);
}

TEST_CASE("Test custom processor", "[TestCutomProcessor]") {
  TestController testController;

  char src_format[] = "/tmp/gt.XXXXXX";
  const char *sourcedir = testController.createTempDirectory(src_format);

  create_testfile_for_getfile(sourcedir);

  add_custom_processor("myproc", custom_processor_logic);

  auto instance = create_instance_obj();
  REQUIRE(instance != nullptr);
  flow *test_flow = create_new_flow(instance);
  REQUIRE(test_flow != nullptr);

  processor *get_proc = add_processor(test_flow, "GetFile");
  REQUIRE(get_proc != nullptr);

  REQUIRE(set_property(get_proc, "Input Directory", sourcedir) == 0);

  processor *my_proc = add_processor(test_flow, "myproc");
  REQUIRE(my_proc != nullptr);

  REQUIRE(set_property(my_proc, "Some test propery", "test value") == 0);

  flow_file_record *record = get_next_flow_file(instance, test_flow);

  REQUIRE(record != nullptr);
}

TEST_CASE("C API robustness test", "[TestRobustness]") {
  free_flow(nullptr);
  free_standalone_processor(nullptr);
  free_instance(nullptr);

  REQUIRE(create_processor(nullptr) == nullptr);

  standalone_processor *standalone_proc = create_processor("GetFile");
  REQUIRE(standalone_proc != nullptr);

  REQUIRE(set_property(nullptr, "prop_name", "prop_value") == -1);

  REQUIRE(set_standalone_property(standalone_proc, nullptr, "prop_value") == -1);

  REQUIRE(set_standalone_property(standalone_proc, "prop_name", nullptr) == -1);

  free_standalone_processor(standalone_proc);

  const char *file_path = "path/to/file";

  flow_file_record *ffr = create_ff_object_na(file_path, strlen(file_path), 0);

  const char *custom_value = "custom value";

  REQUIRE(add_attribute(nullptr, "custom attribute", (void *) custom_value, strlen(custom_value)) == -1);

  REQUIRE(add_attribute(ffr, "custom attribute", (void *) custom_value, strlen(custom_value)) == -1);

  REQUIRE(add_attribute(ffr, nullptr, (void *) custom_value, strlen(custom_value)) == -1);

  REQUIRE(add_attribute(ffr, "custom attribute", nullptr, 0) == -1);

  update_attribute(nullptr, "custom attribute", (void *) custom_value, strlen(custom_value));

  update_attribute(ffr, nullptr, (void *) custom_value, strlen(custom_value));

  attribute attr;
  attr.key = "filename";
  attr.value_size = 0;
  REQUIRE(get_attribute(ffr, &attr) == -1);

  REQUIRE(get_attribute(nullptr, &attr) == -1);

  REQUIRE(get_attribute(ffr, nullptr) == -1);

  REQUIRE(get_attribute_quantity(nullptr) == 0);

  REQUIRE(get_attribute_quantity(ffr) == 0);

  attribute_set attr_set;
  attr_set.size = 3;
  attr_set.attributes = (attribute *) malloc(attr_set.size * sizeof(attribute));  // NOLINT

  REQUIRE(get_all_attributes(nullptr, &attr_set) == 0);

  REQUIRE(get_all_attributes(ffr, &attr_set) == 0);

  REQUIRE(get_all_attributes(ffr, nullptr) == 0);

  free(attr_set.attributes);

  REQUIRE(remove_attribute(nullptr, "key") == -1);

  REQUIRE(remove_attribute(ffr, "key") == -1);

  REQUIRE(remove_attribute(ffr, nullptr) == -1);

  auto instance = create_instance_obj();

  REQUIRE(transmit_flowfile(nullptr, instance) == -1);

  REQUIRE(transmit_flowfile(ffr, nullptr) == -1);

  REQUIRE(create_new_flow(nullptr) == nullptr);

  flow *test_flow = create_new_flow(instance);

  REQUIRE(test_flow != nullptr);

  REQUIRE(add_processor(nullptr, "GetFile") == nullptr);

  REQUIRE(add_processor(test_flow, nullptr) == nullptr);

  free_flow(test_flow);

  free_instance(instance);
}

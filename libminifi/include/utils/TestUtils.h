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

#pragma once

#include <string>
#include <memory>

#include "../../test/TestBase.h"
#include "utils/file/FileUtils.h"
#include "utils/Environment.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string createTempDir(TestController* testController) {
  char dirtemplate[] = "/tmp/gt.XXXXXX";
  std::string temp_dir = testController->createTempDirectory(dirtemplate);
  REQUIRE(!temp_dir.empty());
  REQUIRE(file::FileUtils::is_directory(temp_dir.c_str()));
  return temp_dir;
}

std::string putFileToDir(const std::string& dir_path, const std::string& file_name, const std::string& content) {
  std::string file_path(file::FileUtils::concat_path(dir_path, file_name));
  std::ofstream out_file(file_path, std::ios::binary | std::ios::out);
  if (out_file.is_open()) {
    out_file << content;
  }
  return file_path;
}

std::string createTempDirWithFile(TestController* testController, const std::string& file_name, const std::string& content) {
  std::string temp_dir = createTempDir(testController);
  putFileToDir(temp_dir, file_name, content);
  return temp_dir;
}

std::string getFileContent(const std::string& file_name) {
  std::ifstream file_handle(file_name, std::ios::binary | std::ios::in);
  REQUIRE(file_handle.is_open());
  const std::string file_content{ (std::istreambuf_iterator<char>(file_handle)), (std::istreambuf_iterator<char>()) };
  return file_content;
}

Identifier generateUUID() {
  // TODO(hunyadi): Will make the Id generator manage lifetime using a unique_ptr and return a raw ptr on access
  static std::shared_ptr<utils::IdGenerator> id_generator = utils::IdGenerator::getIdGenerator();
  return id_generator->generate();
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

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

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/file/FileSystem.h"

using utils::crypto::EncryptionProvider;
using utils::file::FileSystem;

utils::crypto::Bytes encryption_key = utils::StringUtils::from_hex("4024b327fdc987ce3eb43dd1f690b9987e4072e0020e3edf4349ce1ad91a4e38");

struct FileSystemTest : TestController {
  FileSystemTest() {
    dir = createTempDirectory();
    encrypted_file = utils::file::FileUtils::concat_path(dir, "encrypted.txt");
    raw_file = utils::file::FileUtils::concat_path(dir, "raw.txt");
    new_file = utils::file::FileUtils::concat_path(dir, "new.txt");

    std::ofstream{encrypted_file, std::ios::binary} << crypto.encrypt("banana");
    std::ofstream{raw_file, std::ios::binary} << "banana";
  }

  EncryptionProvider crypto{encryption_key};
  std::string encrypted_file;
  std::string raw_file;
  std::string new_file;
  std::string dir;
};

TEST_CASE_METHOD(FileSystemTest, "Can read encrypted or non-encrypted file", "[file_system]") {
  FileSystem fs{true, crypto};
  REQUIRE(fs.read(encrypted_file) == "banana");
  REQUIRE(fs.read(raw_file) == "banana");
}

TEST_CASE_METHOD(FileSystemTest, "Write encrypted file", "[file_system]") {
  FileSystem fs{true, crypto};

  fs.write(new_file, "red lorry, yellow lorry");

  std::ifstream file{new_file, std::ios::binary};
  std::string file_content{std::istreambuf_iterator<char>(file), {}};
  REQUIRE(crypto.decrypt(file_content) == "red lorry, yellow lorry");
}

TEST_CASE_METHOD(FileSystemTest, "Can read encrypted but writes non-encrypted", "[file_system]") {
  FileSystem fs{false, crypto};
  REQUIRE(fs.read(encrypted_file) == "banana");

  fs.write(new_file, "red lorry, yellow lorry");

  std::ifstream file{new_file, std::ios::binary};
  std::string file_content{std::istreambuf_iterator<char>(file), {}};
  REQUIRE(file_content == "red lorry, yellow lorry");
}

TEST_CASE_METHOD(FileSystemTest, "Can't read encrypted file without encryption provider", "[file_system]") {
  FileSystem fs{false, std::nullopt};
  REQUIRE(fs.read(encrypted_file) != "banana");
}

TEST_CASE_METHOD(FileSystemTest, "Can read and write unencrypted", "[file_system]") {
  FileSystem fs{false, std::nullopt};
  fs.write(new_file, "red lorry, yellow lorry");

  std::ifstream file{new_file, std::ios::binary};
  std::string file_content{std::istreambuf_iterator<char>(file), {}};
  REQUIRE(file_content == "red lorry, yellow lorry");
}

TEST_CASE_METHOD(FileSystemTest, "Required to encrypt but no key was provided", "[file_system]") {
  REQUIRE_THROWS((FileSystem{true, std::nullopt}));
}

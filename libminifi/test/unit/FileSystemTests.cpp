// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/file/FileSystem.h"

using utils::crypto::EncryptionProvider;
using utils::file::FileSystem;

utils::crypto::Bytes encryption_key = utils::string::from_hex("4024b327fdc987ce3eb43dd1f690b9987e4072e0020e3edf4349ce1ad91a4e38");

struct FileSystemTest : TestController {
  FileSystemTest() {
    dir = createTempDirectory();
    encrypted_file = dir / "encrypted.txt";
    raw_file = dir / "raw.txt";
    new_file = dir / "new.txt";

    std::ofstream{encrypted_file, std::ios::binary} << crypto.encrypt("banana");
    std::ofstream{raw_file, std::ios::binary} << "banana";
  }

  EncryptionProvider crypto{encryption_key};
  std::filesystem::path encrypted_file;
  std::filesystem::path raw_file;
  std::filesystem::path new_file;
  std::filesystem::path dir;
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

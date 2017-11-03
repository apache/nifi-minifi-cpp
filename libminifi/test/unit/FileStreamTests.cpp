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
#include "io/FileStream.h"
#include <string>
#include <vector>
#include <iostream>
#include <uuid/uuid.h>
#include "../TestBase.h"

TEST_CASE("TestFileOverWrite", "[TestFiles]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(reinterpret_cast<uint8_t*>(const_cast<char*>("file")), 4);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.readData(verifybuffer, stream.getSize()) == stream.getSize());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempfile");

  unlink(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(reinterpret_cast<uint8_t*>(const_cast<char*>("file")), 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.readData(verifybuffer, stream.getSize()) == stream.getSize());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  unlink(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange2", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(nullptr, 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.readData(verifybuffer, stream.getSize()) == stream.getSize());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  unlink(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange3", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(nullptr, 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.readData(nullptr, stream.getSize()) == -1);

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "");

  unlink(ss.str().c_str());
}

TEST_CASE("TestFileBeyondEnd3", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.readData(verifybuffer, 8192) == 8);

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  unlink(ss.str().c_str());
}

TEST_CASE("TestFileExceedSize", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  for (int i = 0; i < 10240; i++)
    file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.readData(readBuffer, stream.getSize()) == stream.getSize());

  uint8_t* data = readBuffer.data();

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  for (int i = 0; i < 10; i++)
    REQUIRE(stream.readData(verifybuffer, 8192) == 8192);
  REQUIRE(stream.readData(verifybuffer, 8192) == 0);
  stream.seek(0);
  for (int i = 0; i < 10; i++)
    REQUIRE(stream.readData(verifybuffer, 8192) == 8192);
  REQUIRE(stream.readData(verifybuffer, 8192) == 0);

  unlink(ss.str().c_str());
}

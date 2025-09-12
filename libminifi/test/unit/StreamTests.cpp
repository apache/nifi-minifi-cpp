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

#include <thread>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/io/BaseStream.h"
#include "io/StreamSlice.h"
#include "utils/span.h"

TEST_CASE("TestReadData", "[testread]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  base->write(reinterpret_cast<const uint8_t*>("\x01\x02\x03\x04\x05\x06\x07\x08"), 8);
  uint64_t c = 0;
  REQUIRE(8 == base->read(c));
  REQUIRE(c == 0x0102030405060708);
}

TEST_CASE("TestRead8", "[testread]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  uint64_t b = 8;
  base->write(b);
  uint64_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestRead2", "[testread]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  uint16_t b = 8;
  base->write(b);
  uint16_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestRead1", "[testread]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  uint8_t b = 8;
  base->write(&b, 1);
  uint8_t c = 0;
  REQUIRE(1 == base->read(c));
  REQUIRE(c == 8);
}

TEST_CASE("TestRead4", "[testread]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  uint32_t b = 8;
  base->write(b);
  uint32_t c = 0;
  base->read(c);
  REQUIRE(c == 8);
}

TEST_CASE("TestWrite1", "[testwrite]") {
  auto base = std::make_shared<minifi::io::BufferStream>();
  base->write(static_cast<uint64_t>(0x0102030405060708));
  std::string bytes(8, '\0');
  REQUIRE(8 == base->read(as_writable_bytes(std::span(bytes))));
  REQUIRE(bytes == "\x01\x02\x03\x04\x05\x06\x07\x08");
}

TEST_CASE("InvalidStreamSliceTest", "[teststreamslice]") {
  std::shared_ptr<minifi::io::BaseStream> base = std::make_shared<minifi::io::BufferStream>();
  base->write(reinterpret_cast<const uint8_t*>("\x01\x02\x03\x04\x05\x06\x07\x08"), 8);
  auto input_stream = std::static_pointer_cast<minifi::io::InputStream>(base);
  REQUIRE_THROWS_WITH(std::make_shared<minifi::io::StreamSlice>(input_stream, 0, 9), "StreamSlice is bigger than the Stream, Stream size: 8, StreamSlice size: 9, offset: 0");
  REQUIRE_THROWS_WITH(std::make_shared<minifi::io::StreamSlice>(input_stream, 7, 3), "StreamSlice is bigger than the Stream, Stream size: 8, StreamSlice size: 3, offset: 7");
}

TEST_CASE("StreamSliceTest1", "[teststreamslice]") {
  std::shared_ptr<minifi::io::BaseStream> base = std::make_shared<minifi::io::BufferStream>();
  base->write(reinterpret_cast<const uint8_t*>("\x01\x02\x03\x04\x05\x06\x07\x08"), 8);
  auto input_stream = std::static_pointer_cast<minifi::io::InputStream>(base);
  std::shared_ptr<minifi::io::InputStream> stream_slice = std::make_shared<minifi::io::StreamSlice>(input_stream, 2, 4);
  std::vector<std::byte> buffer;
  buffer.resize(stream_slice->size());
  REQUIRE(stream_slice->read(buffer) == 4);
  stream_slice->seek(0);
  std::vector<std::byte> buffer2;
  buffer2.resize(1000);
  REQUIRE(stream_slice->read(buffer2) == 4);
  buffer2.resize(4);
  REQUIRE(buffer == buffer2);
  REQUIRE(utils::span_to<std::vector>(utils::as_span<uint8_t>(std::span(buffer))) == std::vector<uint8_t>({3, 4, 5, 6}));
}

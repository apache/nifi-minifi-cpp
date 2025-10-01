/**
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
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "sitetosite/CompressionOutputStream.h"
#include "sitetosite/CompressionInputStream.h"
#include "io/BufferStream.h"
#include "io/ZlibStream.h"

namespace org::apache::nifi::minifi::test {

void verifySyncBytes(io::BufferStream& buffer_stream) {
  std::vector<std::byte> data_buffer;
  data_buffer.resize(4);
  buffer_stream.read(std::span(data_buffer));
  REQUIRE(std::to_integer<char>(data_buffer[0]) == sitetosite::SYNC_BYTES[0]);
  REQUIRE(std::to_integer<char>(data_buffer[1]) == sitetosite::SYNC_BYTES[1]);
  REQUIRE(std::to_integer<char>(data_buffer[2]) == sitetosite::SYNC_BYTES[2]);
  REQUIRE(std::to_integer<char>(data_buffer[3]) == sitetosite::SYNC_BYTES[3]);
}

void verifyOriginalSize(io::BufferStream& buffer_stream, uint32_t expected_size) {
  uint32_t original_size = 0;
  buffer_stream.read(original_size);
  REQUIRE(original_size == expected_size);
}

void verifyCompressedData(io::BufferStream& compressed_buffer_stream, uint32_t expected_size) {
  uint32_t compressed_size = 0;
  compressed_buffer_stream.read(compressed_size);

  std::vector<std::byte> compressed_data_buffer;
  compressed_data_buffer.resize(compressed_size);
  compressed_buffer_stream.read(std::span(compressed_data_buffer));

  io::BufferStream decompressed_data_stream;
  io::ZlibDecompressStream decompressor(gsl::make_not_null(&decompressed_data_stream), io::ZlibCompressionFormat::ZLIB);
  decompressor.write(compressed_data_buffer);
  REQUIRE(decompressor.isFinished());

  std::vector<std::byte> decompressed_data_buffer;
  decompressed_data_buffer.resize(expected_size);
  decompressed_data_stream.read(std::span(decompressed_data_buffer));

  for (size_t i = 0; i < decompressed_data_buffer.size(); i += 4) {
    uint32_t value = (static_cast<uint32_t>(std::to_integer<uint8_t>(decompressed_data_buffer[i])) << 24)
      | (static_cast<uint32_t>(std::to_integer<uint8_t>(decompressed_data_buffer[i + 1])) << 16)
      | (static_cast<uint32_t>(std::to_integer<uint8_t>(decompressed_data_buffer[i + 2])) << 8)
      | static_cast<uint32_t>(std::to_integer<uint8_t>(decompressed_data_buffer[i + 3]));

    REQUIRE(value == 42);
}
}

void verifyContinueByte(io::BufferStream& buffer_stream) {
  uint8_t closing_byte = 2;
  buffer_stream.read(closing_byte);
  REQUIRE(closing_byte == 1);
}

void verifyClosingByte(io::BufferStream& buffer_stream) {
  uint8_t closing_byte = 2;
  buffer_stream.read(closing_byte);
  REQUIRE(closing_byte == 0);
}

void verifyCompressedChunks(io::BufferStream& buffer_stream, uint32_t expected_size) {
  bool first_chunk = true;
  uint32_t size_processed = 0;
  while (size_processed < expected_size) {
    uint32_t current_size_to_read = 0;
    if (expected_size - size_processed > sitetosite::COMPRESSION_BUFFER_SIZE) {
      current_size_to_read = sitetosite::COMPRESSION_BUFFER_SIZE;
    } else {
      current_size_to_read = expected_size - size_processed;
    }
    if (first_chunk) {
      first_chunk = false;
    } else {
      verifyContinueByte(buffer_stream);
    }
    verifySyncBytes(buffer_stream);
    verifyOriginalSize(buffer_stream, current_size_to_read);
    verifyCompressedData(buffer_stream, current_size_to_read);
    size_processed += current_size_to_read;
  }
  verifyClosingByte(buffer_stream);
}

TEST_CASE("Write empty output stream", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  output_stream.close();
  REQUIRE(buffer_stream.size() == 0);
}

TEST_CASE("Write a 4 byte integer and flush", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  output_stream.flush();
  verifyCompressedChunks(buffer_stream, 4);
}

TEST_CASE("Write a single chunk of compressed data and flush on close", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }
  REQUIRE(buffer_stream.size() == 0);
  output_stream.close();
  REQUIRE(buffer_stream.size() > 0);

  verifyCompressedChunks(buffer_stream, 40000);
}

TEST_CASE("Write 2 chunks of compressed data and flush on demand", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }
  REQUIRE(buffer_stream.size() == 0);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }

  // Automatically compress data when buffer is full
  REQUIRE(buffer_stream.size() > 0);
  output_stream.close();

  verifyCompressedChunks(buffer_stream, 80000);
}

TEST_CASE("Write 3 chunks of compressed data and flush on demand", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }
  REQUIRE(buffer_stream.size() == 0);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }

  // Automatically compress data when buffer is full
  REQUIRE(buffer_stream.size() > 0);
  for (size_t i = 0; i < 20000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }
  output_stream.close();

  verifyCompressedChunks(buffer_stream, 160000);
}

TEST_CASE("Read single 4 byte integer compressed", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  output_stream.flush();
  sitetosite::CompressionInputStream input_stream(buffer_stream);
  uint32_t read_byte{};
  CHECK(input_stream.read(read_byte) == 4);
  CHECK(read_byte == 42);
}

TEST_CASE("Read large number of bytes compressed", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  for (size_t i = 0; i < 10000; ++i) {
    CHECK(output_stream.write(static_cast<uint32_t>(42)) == 4);
  }
  output_stream.flush();
  sitetosite::CompressionInputStream input_stream(buffer_stream);
  for (size_t i = 0; i < 10000; ++i) {
    uint32_t read_byte{};
    CHECK(input_stream.read(read_byte) == 4);
    CHECK(read_byte == 42);
  }
}

TEST_CASE("Read large number of bytes that uses multiple buffers", "[CompressionOutputStream]") {
  io::BufferStream buffer_stream;
  sitetosite::CompressionOutputStream output_stream(buffer_stream);
  uint32_t count = 0;
  while (buffer_stream.size() + 100 < sitetosite::COMPRESSION_BUFFER_SIZE) {
    ++count;
    CHECK(output_stream.write(count) == 4);
  }
  output_stream.flush();

  sitetosite::CompressionInputStream input_stream(buffer_stream);
  for (size_t i = 1; i <= count; ++i) {
    uint32_t read_byte{};
    CHECK(input_stream.read(read_byte) == 4);
    CHECK(read_byte == i);
  }
}

}  // namespace org::apache::nifi::minifi::test

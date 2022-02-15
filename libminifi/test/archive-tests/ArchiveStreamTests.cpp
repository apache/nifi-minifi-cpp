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

#include "../TestBase.h"
#include "../Catch.h"
#include "WriteArchiveStream.h"
#include "ReadArchiveStream.h"

using namespace minifi;

TEST_CASE("Create and read archive") {
  std::map<std::string, std::string> files{
      {"a.txt", "hello, I'm file A"},
      {"b.txt", "hello, I'm file B"}
  };

  auto archive = std::make_shared<io::BufferStream>();

  {
    io::WriteArchiveStreamImpl compressor(9, io::CompressionFormat::GZIP, archive);

    for (const auto& [filename, content] : files) {
      REQUIRE(compressor.newEntry({filename, content.length()}));
      REQUIRE(compressor.write(reinterpret_cast<const uint8_t*>(content.data()), content.length()) == content.length());
    }
  }

  {
    io::ReadArchiveStreamImpl decompressor(archive);

    size_t extracted_entries = 0;
    while (auto info = decompressor.nextEntry()) {
      ++extracted_entries;
      std::string file_content;
      file_content.resize(info->size);
      REQUIRE(decompressor.read(minifi::gsl::make_span(file_content).as_span<std::byte>()) == file_content.length());
      REQUIRE(files[info->filename] == file_content);
    }
    REQUIRE(extracted_entries == files.size());
  }
}

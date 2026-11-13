// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "WriteArchiveStream.h"
#include "ReadArchiveStream.h"

TEST_CASE("Create and read archive") {
  std::map<std::string, std::string> files{
      {"a.txt", "hello, I'm file A"},
      {"b.txt", "hello, I'm file B"}
  };

  auto archive = std::make_shared<minifi::io::BufferStream>();

  {
    minifi::io::WriteArchiveStreamImpl compressor(9, minifi::io::CompressionFormat::GZIP, archive);

    for (const auto& [filename, content] : files) {
      REQUIRE(compressor.newEntry({filename, content.length()}));
      REQUIRE(compressor.write(reinterpret_cast<const uint8_t*>(content.data()), content.length()) == content.length());
    }
  }

  {
    minifi::io::ReadArchiveStreamImpl decompressor(archive);

    size_t extracted_entries = 0;
    while (auto info = decompressor.nextEntry()) {
      ++extracted_entries;
      std::string file_content;
      file_content.resize(info->size);
      REQUIRE(decompressor.read(as_writable_bytes(std::span(file_content))) == file_content.length());
      REQUIRE(files[info->filename] == file_content);
    }
    REQUIRE(extracted_entries == files.size());
  }
}

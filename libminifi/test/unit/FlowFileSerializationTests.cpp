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
#include <algorithm>
#include <memory>
#include <string>

#include "io/BaseStream.h"
#include "serialization/FlowFileV3Serializer.h"
#include "serialization/PayloadSerializer.h"
#include "core/FlowFile.h"
#include "../TestBase.h"

TEST_CASE("Payload Serializer", "[testPayload]") {
  std::string content = "flowFileContent";
  auto contentStream = std::make_shared<minifi::io::BaseStream>();
  contentStream->write(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(content.data())), content.length());

  auto result = std::make_shared<minifi::io::BaseStream>();

  auto flowFile = std::make_shared<minifi::FlowFileRecord>(nullptr, nullptr);
  flowFile->setSize(content.size());
  flowFile->addAttribute("first", "one");
  flowFile->addAttribute("second", "two");

  minifi::PayloadSerializer serializer([&] (const std::shared_ptr<core::FlowFile>&, minifi::InputStreamCallback* cb) {
    return cb->process(contentStream);
  });
  serializer.serialize(flowFile, result);

  std::string serialized{reinterpret_cast<const char*>(result->getBuffer()), result->getSize()};

  REQUIRE(serialized == content);
}

TEST_CASE("FFv3 Serializer", "[testFFv3]") {
  std::string content = "flowFileContent";
  auto contentStream = std::make_shared<minifi::io::BaseStream>();
  contentStream->write(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(content.data())), content.length());

  auto result = std::make_shared<minifi::io::BaseStream>();

  auto flowFile = std::make_shared<minifi::FlowFileRecord>(nullptr, nullptr);
  flowFile->setSize(content.size());
  flowFile->addAttribute("first", "one");
  flowFile->addAttribute("second", "two");

  minifi::FlowFileV3Serializer serializer([&] (const std::shared_ptr<core::FlowFile>&, minifi::InputStreamCallback* cb) {
    return cb->process(contentStream);
  });
  serializer.serialize(flowFile, result);

  std::string serialized{reinterpret_cast<const char*>(result->getBuffer()), result->getSize()};

  std::string expected = "NiFiFF3";
  expected += std::string{"\x00\x02", 2};  // number of attributes
  expected += std::string{"\x00\x05", 2} + "first";  // first key
  expected += std::string{"\x00\x03", 2} + "one";  // first value
  expected += std::string{"\x00\x06", 2} + "second";  // second key
  expected += std::string{"\x00\x03", 2} + "two";  // second value
  expected += std::string{"\x00\x00\x00\x00\x00\x00\x00\x0f", 8} + content;  // payload of the flowFile

  REQUIRE(serialized == expected);
}


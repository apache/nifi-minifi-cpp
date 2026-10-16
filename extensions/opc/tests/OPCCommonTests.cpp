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
#include <array>
#include <string>

#include "catch2/generators/catch_generators.hpp"
#include "include/OPCCommon.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("An empty variant cannot be converted to a string", "[opccommon]") {
  UA_Variant variant;
  UA_Variant_init(&variant);

  CHECK_THROWS_WITH(opc::variantToString(variant, opc::BinaryEncoding::Raw), "General Operation: Cannot convert an empty variant to string");
}

TEST_CASE("A variant of an unsupported type cannot be converted to a string", "[opccommon]") {
  UA_Range range{.low = 0.0, .high = 100.0};
  UA_Variant variant;
  UA_Variant_setScalar(&variant, &range, &UA_TYPES[UA_TYPES_RANGE]);

  CHECK_THROWS_WITH(opc::variantToString(variant, opc::BinaryEncoding::Raw), "General Operation: Data type is not supported: Range");
}

TEST_CASE("The binary encoding does not change the conversion of non-binary values", "[opccommon]") {
  const auto binary_encoding = GENERATE(opc::BinaryEncoding::Raw, opc::BinaryEncoding::Base64);

  SECTION("String") {
    UA_String value = UA_STRING_STATIC("some text");
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_STRING]);

    CHECK(opc::variantToString(variant, binary_encoding) == "some text");
  }

  SECTION("Byte") {
    UA_Byte value = 255;
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_BYTE]);

    CHECK(opc::variantToString(variant, binary_encoding) == "255");
  }

  SECTION("SByte") {
    UA_SByte value = -128;
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_SBYTE]);

    CHECK(opc::variantToString(variant, binary_encoding) == "-128");
  }

  SECTION("LocalizedText") {
    UA_LocalizedText value = UA_LOCALIZEDTEXT(const_cast<char*>("en-US"), const_cast<char*>("some message"));
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);

    CHECK(opc::variantToString(variant, binary_encoding) == "some message");
  }

  SECTION("NodeId") {
    UA_NodeId value = UA_NODEID_NUMERIC(1, 42);
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_NODEID]);

    CHECK(opc::variantToString(variant, binary_encoding) == "ns=1;i=42");
  }

  SECTION("ExpandedNodeId") {
    UA_ExpandedNodeId value = UA_EXPANDEDNODEID_NUMERIC(1, 42);
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_EXPANDEDNODEID]);

    CHECK(opc::variantToString(variant, binary_encoding) == "ns=1;i=42");
  }

  SECTION("Guid") {
    UA_Guid value = UA_GUID("72962B91-FA75-4AE6-8D28-B404DC7DAF63");
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_GUID]);

    CHECK(opc::variantToString(variant, binary_encoding) == "72962b91-fa75-4ae6-8d28-b404dc7daf63");
  }

  SECTION("QualifiedName") {
    UA_QualifiedName value = UA_QUALIFIEDNAME(1, const_cast<char*>("Colour"));
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);

    CHECK(opc::variantToString(variant, binary_encoding) == "1:Colour");
  }

  SECTION("StatusCode") {
    UA_StatusCode value = UA_STATUSCODE_BADNODEIDUNKNOWN;
    UA_Variant variant;
    UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_STATUSCODE]);

    CHECK(opc::variantToString(variant, binary_encoding) == "BadNodeIdUnknown");
  }
}

TEST_CASE("A byte string variant is converted according to the requested binary encoding", "[opccommon]") {
  std::array<uint8_t, 4> bytes{0x00, 0x0f, 0xa0, 0xff};
  UA_ByteString value{bytes.size(), bytes.data()};
  UA_Variant variant;
  UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_BYTESTRING]);

  SECTION("Raw keeps the bytes as they are") {
    const std::string expected{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    CHECK(opc::variantToString(variant, opc::BinaryEncoding::Raw) == expected);
  }

  SECTION("Base64 encodes the bytes") {
    CHECK(opc::variantToString(variant, opc::BinaryEncoding::Base64) == "AA+g/w==");
  }
}

TEST_CASE("An empty byte string variant is converted to an empty string", "[opccommon]") {
  const auto binary_encoding = GENERATE(opc::BinaryEncoding::Raw, opc::BinaryEncoding::Base64);
  UA_ByteString value = UA_BYTESTRING_NULL;
  UA_Variant variant;
  UA_Variant_setScalar(&variant, &value, &UA_TYPES[UA_TYPES_BYTESTRING]);

  CHECK(opc::variantToString(variant, binary_encoding).empty());
}

}  // namespace org::apache::nifi::minifi::test

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

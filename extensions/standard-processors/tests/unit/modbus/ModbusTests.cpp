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
#include <numbers>

#include "modbus/ReadModbusFunctions.h"
#include "Catch.h"
namespace org::apache::nifi::minifi::modbus::test {

template <typename... Bytes>
std::vector<std::byte> createByteVector(Bytes... bytes) {
  return {static_cast<std::byte>(bytes)...};
}

TEST_CASE("ReadCoilStatus") {
  const auto read_coil_status = ReadCoilStatus(280, 0, 19, 19);
  {
    {
      CHECK(read_coil_status.rawPdu() == createByteVector(0x01, 0x00, 0x13, 0x00, 0x13));
      CHECK(read_coil_status.requestBytes() == createByteVector(0x01, 0x18, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x13, 0x00, 0x13));
    }

    auto serialized_response = read_coil_status.responseToRecordField(createByteVector(0x01, 0x03, 0xCD, 0x6B, 0x05));
    REQUIRE(serialized_response.has_value());
    auto record_array = core::RecordArray();
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(true);
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(false);
    record_array.emplace_back(true);
    record_array.emplace_back(true);
    record_array.emplace_back(true);
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(true);
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(true);
    record_array.emplace_back(false);
    record_array.emplace_back(true);

    CHECK(std::get<core::RecordArray>(serialized_response->value_) == record_array);
  }

  {
    auto shorter_than_expected_resp = read_coil_status.responseToRecordField(createByteVector(0x01, 0x02, 0xCD, 0x6B));
    REQUIRE(!shorter_than_expected_resp);
    CHECK_THAT(shorter_than_expected_resp.error(), minifi::test::MatchesError(modbus::ModbusExceptionCode::InvalidResponse));
  }


  {
    auto longer_than_expected_resp = read_coil_status.responseToRecordField(createByteVector(0x01, 0x04, 0xCD, 0x6B, 0x05, 0x07));
    REQUIRE(!longer_than_expected_resp);
    CHECK_THAT(longer_than_expected_resp.error(), minifi::test::MatchesError(modbus::ModbusExceptionCode::InvalidResponse));
  }

  {
    auto mismatching_size_resp = read_coil_status.responseToRecordField(createByteVector(0x01, 0x03, 0xCD, 0x6B, 0x05, 0x07));
    REQUIRE(!mismatching_size_resp);
    CHECK_THAT(mismatching_size_resp.error(), minifi::test::MatchesError(modbus::ModbusExceptionCode::InvalidResponse));
  }
}

TEST_CASE("ReadHoldingRegisters uint16_t") {
  {
    const auto read_holding_registers = ReadRegisters<uint16_t>(RegisterType::holding, 0, 0, 5, 3);
    {
      CHECK(read_holding_registers.rawPdu() == createByteVector(0x03, 0x00, 0x05, 0x00, 0x03));
    }

    auto serialized_response = read_holding_registers.responseToRecordField(createByteVector(0x03, 0x06, 0x3A, 0x98, 0x13, 0x88, 0x00, 0xC8));
    REQUIRE(serialized_response.has_value());
    auto record_array = core::RecordArray();
    record_array.emplace_back(15000);
    record_array.emplace_back(5000);
    record_array.emplace_back(200);
    CHECK(std::get<core::RecordArray>(serialized_response->value_) == record_array);
  }
}

TEST_CASE("ReadHoldingRegisters char") {
  {
    const auto read_holding_registers = ReadRegisters<char>(RegisterType::holding, 0, 0, 5, 3);
    {
      CHECK(read_holding_registers.rawPdu() == createByteVector(0x03, 0x00, 0x05, 0x00, 0x03));
    }

    auto serialized_response = read_holding_registers.responseToRecordField(createByteVector(0x03, 0x06, 0x00, 0x66, 0x00, 0x6F, 0x00, 0x6F));
    REQUIRE(serialized_response.has_value());
    auto record_array = core::RecordArray();
    record_array.emplace_back('f');
    record_array.emplace_back('o');
    record_array.emplace_back('o');
    CHECK(std::get<core::RecordArray>(serialized_response->value_) == record_array);
  }
}

TEST_CASE("ReadInputRegisters") {
  {
    const auto read_input_registers = ReadRegisters<uint16_t>(RegisterType::input, 0, 0, 5, 3);
    {
      CHECK(read_input_registers.rawPdu() == createByteVector(0x04, 0x00, 0x05, 0x00, 0x03));
    }
    auto serialized_response = read_input_registers.responseToRecordField(createByteVector(0x04, 0x06, 0x3A, 0x98, 0x13, 0x88, 0x00, 0xC8));
    REQUIRE(serialized_response.has_value());
    auto record_array = core::RecordArray();
    record_array.emplace_back(15000);
    record_array.emplace_back(5000);
    record_array.emplace_back(200);
    CHECK(std::get<core::RecordArray>(serialized_response->value_) == record_array);
  }
}

TEST_CASE("ByteConversion") {
  {
    constexpr std::array from{std::byte{0x12}, std::byte{0x24}};

    CHECK(4644 == modbus::fromBytes<uint16_t>(from));
    CHECK(9234 == modbus::fromBytes<uint16_t, std::endian::little>(from));
  }

  {
    constexpr std::array from{std::byte{0x00}, std::byte{0x61}};
    CHECK('a' == modbus::fromBytes<char>(from));
  }

  {
    constexpr std::array from{std::byte{0x61}, std::byte{0x00}};
    CHECK('\0' == modbus::fromBytes<char>(from));
  }

  {
    constexpr std::array from{std::byte{0x1A}, std::byte{0x45}, std::byte{0x02}, std::byte{0x3F}};
    CHECK(440730175 == modbus::fromBytes<uint32_t>(from));
  }

  {
    constexpr std::array from{std::byte{0x40}, std::byte{0xD8}, std::byte{0xF5}, std::byte{0xC3}};
    CHECK(6.78F == modbus::fromBytes<float>(from));
  }

  {
    constexpr std::array<std::byte, 8> pi_double{
      std::byte{0x40}, std::byte{0x09}, std::byte{0x21}, std::byte{0xFB},
      std::byte{0x54}, std::byte{0x44}, std::byte{0x2d}, std::byte{0x18}};
    CHECK(std::numbers::pi == modbus::fromBytes<double>(pi_double));
  }
}

TEST_CASE("ParseAddress") {
  constexpr uint16_t transaction_id = 1;
  constexpr uint8_t unit_id = 0;
  {
    auto expected = ReadRegisters<uint16_t>(RegisterType::holding, transaction_id, unit_id, 20, 10);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "holding-register:20:UINT[10]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "400020:UINT[10]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x00020:UINT[10]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "40020:UINT[10]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x0020:UINT[10]") == expected);
  }

  {
    auto expected = ReadRegisters<uint16_t>(RegisterType::holding, transaction_id, unit_id, 5678, 1);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "holding-register:5678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "405678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x05678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "45678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x5678") == expected);
  }

  {
    auto expected = ReadRegisters<uint16_t>(RegisterType::input, transaction_id, unit_id, 5678, 1);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "input-register:5678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "305678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "3x05678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "35678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "3x5678") == expected);
  }

  {
    auto expected = ReadRegisters<char>(RegisterType::holding, transaction_id, unit_id, 5678, 1);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "holding-register:5678:CHAR") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "405678:CHAR") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x05678:CHAR") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "45678:CHAR") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x5678:CHAR") == expected);
  }

  {
    auto expected = ReadRegisters<float>(RegisterType::holding, transaction_id, unit_id, 7777, 2);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "holding-register:7777:REAL[2]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "407777:REAL[2]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x07777:REAL[2]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "47777:REAL[2]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "4x7777:REAL[2]") == expected);
  }

  {
    auto expected = ReadRegisters<uint16_t>(RegisterType::input, transaction_id, unit_id, 5678, 1);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "input-register:5678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "305678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "3x05678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "35678") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "3x5678") == expected);
  }

  {
    auto expected = ReadCoilStatus(transaction_id, unit_id, 4234, 1);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "coil:4234") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "104234") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "1x04234") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "14234") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "1x4234") == expected);
  }

  {
    auto expected = ReadCoilStatus(transaction_id, unit_id, 222, 12);

    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "coil:222[12]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "100222[12]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "1x00222[12]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "10222[12]") == expected);
    CHECK(*ReadModbusFunction::parse(transaction_id, unit_id, "1x0222[12]") == expected);
  }
}

}  // namespace org::apache::nifi::minifi::modbus::test

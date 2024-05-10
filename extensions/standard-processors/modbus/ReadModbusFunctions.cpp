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

#include "ReadModbusFunctions.h"

namespace org::apache::nifi::minifi::modbus {
std::vector<std::byte> ReadModbusFunction::requestBytes() const {
  constexpr std::array modbus_service_protocol_identifier = {std::byte{0}, std::byte{0}};
  const auto pdu = rawPdu();
  const uint16_t length = pdu.size() + 1;

  std::vector<std::byte> request;
  ranges::copy(toBytes(transaction_id_), std::back_inserter(request));
  ranges::copy(modbus_service_protocol_identifier, std::back_inserter(request));
  ranges::copy(toBytes(length), std::back_inserter(request));
  request.push_back(std::byte{unit_id_});
  ranges::copy(pdu, std::back_inserter(request));
  return request;
}

[[nodiscard]] auto ReadModbusFunction::getRespBytes(std::span<const std::byte> resp_pdu) const -> nonstd::expected<std::span<const std::byte>, std::error_code> {
  if (resp_pdu.size() < 2) {
    return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
  }

  if (const auto resp_function_code = resp_pdu.front(); resp_function_code != getFunctionCode()) {
    return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
  }

  const auto resp_byte_count = static_cast<uint8_t>(resp_pdu[1]);
  constexpr uint8_t function_code_length = 1;
  constexpr uint8_t unit_id_length = 1;
  const uint8_t expected_resp_pdu_size = resp_byte_count + function_code_length + unit_id_length;
  if (resp_pdu.size() != expected_resp_pdu_size) {
    return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
  }

  if (resp_byte_count != expectedByteCount()) {
    return nonstd::make_unexpected(ModbusExceptionCode::InvalidResponse);
  }

  return resp_pdu.subspan(2, resp_pdu.size()-2);
}

[[nodiscard]] std::vector<std::byte> ReadCoilStatus::rawPdu() const {
  std::vector<std::byte> result;
  result.push_back(getFunctionCode());
  ranges::copy(toBytes(starting_address_), std::back_inserter(result));
  ranges::copy(toBytes(number_of_points_), std::back_inserter(result));
  return result;
}

[[nodiscard]] nonstd::expected<core::RecordField, std::error_code> ReadCoilStatus::responseToRecordField(const std::span<const std::byte> resp_pdu) const {
  const auto resp_bytes = getRespBytes(resp_pdu);
  if (!resp_bytes)
    return nonstd::make_unexpected(resp_bytes.error());


  std::vector<bool> coils{};
  for (const auto& resp_byte : *resp_bytes) {
    for (uint8_t i = 0; i < 8; ++i) {
      if (coils.size() == number_of_points_) {
        break;
      }
      const bool bit_value = static_cast<bool>((resp_byte & (std::byte{1} << i)) >> i);
      coils.push_back(bit_value);
    }
  }
  if (coils.size() == 1) {
    const bool val = coils.at(0);
    return core::RecordField{val};
  }
  core::RecordArray array;
  for (bool coil : coils) {
    array.emplace_back(coil);
  }
  return core::RecordField{std::move(array)};
}

[[nodiscard]] std::byte ReadCoilStatus::getFunctionCode() const {
  return std::byte{0x01};
}

[[nodiscard]] uint8_t ReadCoilStatus::expectedByteCount() const {
  return number_of_points_ / 8 + (number_of_points_ % 8 != 0);
}

bool ReadCoilStatus::operator==(const ReadModbusFunction& rhs) const {
  const auto read_coil_rhs = dynamic_cast<const ReadCoilStatus*>(&rhs);
  if (!read_coil_rhs)
    return false;

  return read_coil_rhs->transaction_id_ == this->transaction_id_ &&
         read_coil_rhs->starting_address_ == this->starting_address_ &&
         read_coil_rhs->number_of_points_ == this->number_of_points_;
}

std::unique_ptr<ReadModbusFunction> ReadCoilStatus::parse(const uint16_t transaction_id, const uint8_t unit_id, const std::string_view address_str, const std::string_view length_str) {
  auto start_address = utils::string::parse<uint16_t>(address_str);
  if (!start_address) {
    return nullptr;
  }
  uint16_t length = length_str.empty() ? 1 : utils::string::parse<uint16_t>(length_str).value_or(1);

  return std::make_unique<ReadCoilStatus>(transaction_id, unit_id, *start_address, length);
}

namespace {
std::unique_ptr<ReadModbusFunction> parseReadRegister(const RegisterType register_type,
  const uint16_t transaction_id,
  const uint8_t unit_id,
  const std::string_view address_str,
  const std::string_view type_str,
  const std::string_view length_str) {
  auto start_address = utils::string::parse<uint16_t>(address_str);
  if (!start_address) {
    return nullptr;
  }
  uint16_t length = length_str.empty() ? 1 : utils::string::parse<uint16_t>(length_str).value_or(1);

  if (type_str.empty() || type_str == "UINT" || type_str == "WORD") {
    return std::make_unique<ReadRegisters<uint16_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "BOOL") {
    return std::make_unique<ReadRegisters<bool>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "SINT") {
    return std::make_unique<ReadRegisters<int8_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "USINT" || type_str == "BYTE") {
    return std::make_unique<ReadRegisters<uint8_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "INT") {
    return std::make_unique<ReadRegisters<int16_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "DINT") {
    return std::make_unique<ReadRegisters<int32_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "UDINT" || type_str == "DWORD") {
    return std::make_unique<ReadRegisters<uint32_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "LINT") {
    return std::make_unique<ReadRegisters<int64_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "ULINT" || type_str == "LWORD") {
    return std::make_unique<ReadRegisters<uint64_t>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "REAL") {
    return std::make_unique<ReadRegisters<float>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "LREAL") {
    return std::make_unique<ReadRegisters<double>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  if (type_str == "CHAR") {
    return std::make_unique<ReadRegisters<char>>(register_type, transaction_id, unit_id, *start_address, length);
  }

  return nullptr;
}
}  // namespace

std::unique_ptr<ReadModbusFunction> ReadModbusFunction::parse(const uint16_t transaction_id, const uint8_t unit_id, const std::string& address) {
  static const std::regex address_pattern{R"((holding-register|coil|input-register):(\d+)(:([a-zA-Z_]+))?(\[(\d+)\])?)"};

  std::smatch matches;
  if (std::regex_match(address, matches, address_pattern)) {
    if (matches.size() < 7) {
      return nullptr;
    }
    const auto register_type_str = matches[1].str();
    const auto start_address_str = matches[2].str();
    const auto type_str = matches[4].str();
    const auto length_str = matches[6].str();

    if (register_type_str == "coil") {
      return ReadCoilStatus::parse(transaction_id, unit_id, start_address_str, length_str);
    }
    if (register_type_str == "input-register") {
      return parseReadRegister(RegisterType::input, transaction_id, unit_id, start_address_str, type_str, length_str);
    }
    if (register_type_str == "holding-register") {
      return parseReadRegister(RegisterType::holding, transaction_id, unit_id, start_address_str, type_str, length_str);
    }
  }

  static const std::regex address_pattern_short{R"((\dx|\d)(\d{4,5})?(:([a-zA-Z_]+))?(\[(\d+)\])?)"};
  if (std::regex_match(address, matches, address_pattern_short)) {
    if (matches.size() < 7) {
      return nullptr;
    }
    const auto register_type_str = matches[1].str();
    const auto start_address_str = matches[2].str();
    const auto type_str = matches[4].str();
    const auto length_str = matches[6].str();

    if (register_type_str == "1" || register_type_str == "1x") {
      return ReadCoilStatus::parse(transaction_id, unit_id, start_address_str, length_str);
    }
    if (register_type_str == "3" || register_type_str == "3x") {
      return parseReadRegister(RegisterType::input, transaction_id, unit_id, start_address_str, type_str, length_str);
    }
    if (register_type_str == "4" || register_type_str == "4x") {
      return parseReadRegister(RegisterType::holding, transaction_id, unit_id, start_address_str, type_str, length_str);
    }
  }

  return nullptr;
}
}  // namespace org::apache::nifi::minifi::modbus

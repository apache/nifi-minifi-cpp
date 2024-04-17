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

#pragma once

#include <regex>
#include <vector>

#include "fmt/format.h"
#include "modbus/ByteConverters.h"
#include "modbus/Error.h"
#include "range/v3/algorithm/copy.hpp"
#include "range/v3/view/chunk.hpp"
#include "utils/expected.h"
#include "utils/StringUtils.h"
#include "core/RecordField.h"

namespace org::apache::nifi::minifi::modbus {
enum class RegisterType {
  holding,
  input
};

class ReadModbusFunction {
 public:
  explicit ReadModbusFunction(const uint16_t transaction_id, const uint8_t unit_id) : transaction_id_(transaction_id), unit_id_(unit_id) {
  }
  ReadModbusFunction(const ReadModbusFunction&) = delete;
  ReadModbusFunction(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(ReadModbusFunction&&) = delete;
  ReadModbusFunction& operator=(const ReadModbusFunction&) = delete;

  virtual bool operator==(const ReadModbusFunction&) const = 0;

  virtual ~ReadModbusFunction() = default;

  [[nodiscard]] std::vector<std::byte> requestBytes() const {
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

  [[nodiscard]] uint16_t getTransactionId() const { return transaction_id_; }
  [[nodiscard]] uint8_t getUnitId() const { return unit_id_; }
  [[nodiscard]] virtual nonstd::expected<core::RecordField, std::error_code> responseToRecordField(std::span<const std::byte> resp_pdu) const = 0;

  static std::unique_ptr<ReadModbusFunction> parse(uint16_t transaction_id, uint8_t unit_id, const std::string& address);

 protected:
  [[nodiscard]] auto getRespBytes(std::span<const std::byte> resp_pdu) const -> nonstd::expected<std::span<const std::byte>, std::error_code> {
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

  [[nodiscard]] virtual std::byte getFunctionCode() const = 0;
  [[nodiscard]] virtual std::vector<std::byte> rawPdu() const = 0;
  [[nodiscard]] virtual uint8_t expectedByteCount() const = 0;

  const uint16_t transaction_id_;
  const uint8_t unit_id_;
};

class ReadCoilStatus final : public ReadModbusFunction {
 public:
  ReadCoilStatus(const uint16_t transaction_id, const uint8_t unit_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadModbusFunction(transaction_id, unit_id),
        starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(getFunctionCode());
    ranges::copy(toBytes(starting_address_), std::back_inserter(result));
    ranges::copy(toBytes(number_of_points_), std::back_inserter(result));
    return result;
  }

  [[nodiscard]] nonstd::expected<core::RecordField, std::error_code> responseToRecordField(const std::span<const std::byte> resp_pdu) const override {
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

  [[nodiscard]] std::byte getFunctionCode() const override {
    return std::byte{0x01};
  }

  [[nodiscard]] uint8_t expectedByteCount() const override {
    return number_of_points_ / 8 + (number_of_points_ % 8 != 0);
  }

  bool operator==(const ReadCoilStatus& rhs) const = default;

  bool operator==(const ReadModbusFunction& rhs) const override {
    const auto read_coil_rhs = dynamic_cast<const ReadCoilStatus*>(&rhs);
    if (!read_coil_rhs)
      return false;

    return read_coil_rhs->transaction_id_ == this->transaction_id_ &&
           read_coil_rhs->starting_address_ == this->starting_address_ &&
           read_coil_rhs->number_of_points_ == this->number_of_points_;
  }

  static std::unique_ptr<ReadModbusFunction> parse(const uint16_t transaction_id, const uint8_t unit_id, const std::string_view address_str, const std::string_view length_str) {
    auto start_address = utils::string::parse<uint16_t>(address_str);
    if (!start_address) {
      return nullptr;
    }
    uint16_t length = length_str.empty() ? 1 : utils::string::parse<uint16_t>(length_str).value_or(1);

    return std::make_unique<ReadCoilStatus>(transaction_id, unit_id, *start_address, length);
  }

 private:
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

template<typename T>
class ReadRegisters final : public ReadModbusFunction {
 public:
  ReadRegisters(const RegisterType register_type, const uint16_t transaction_id, const uint8_t unit_id, const uint16_t starting_address, const uint16_t number_of_points)
      : ReadModbusFunction(transaction_id, unit_id),
        register_type_(register_type),
        starting_address_(starting_address),
        number_of_points_(number_of_points) {
  }

  [[nodiscard]] std::vector<std::byte> rawPdu() const override {
    std::vector<std::byte> result;
    result.push_back(getFunctionCode());
    ranges::copy(toBytes(starting_address_), std::back_inserter(result));
    ranges::copy(toBytes(wordCount()), std::back_inserter(result));
    return result;
  }

  [[nodiscard]] uint8_t expectedByteCount() const override {
    return number_of_points_*std::max(sizeof(T), sizeof(uint16_t));
  }

  [[nodiscard]] uint16_t wordCount() const {
    return expectedByteCount() / sizeof(uint16_t);
  }

  [[nodiscard]] nonstd::expected<core::RecordField, std::error_code> responseToRecordField(const std::span<const std::byte> resp_pdu) const override {
    const auto resp_bytes = getRespBytes(resp_pdu);
    if (!resp_bytes)
      return nonstd::make_unexpected(resp_bytes.error());

    std::vector<T> holding_registers{};
    for (auto&& register_chunk : ranges::views::chunk(*resp_bytes, std::max(sizeof(T), sizeof(uint16_t)))) {
      std::array<std::byte, std::max(sizeof(T), sizeof(uint16_t))> register_value{};
      ranges::copy(register_chunk, register_value.begin());
      holding_registers.push_back(fromBytes<T>(std::move(register_value)));
    }
    if (holding_registers.size() == 1) {
      T val = holding_registers.at(0);
      return core::RecordField{val};
    }
    core::RecordArray record_array;
    for (auto holding_register : holding_registers) {
      record_array.emplace_back(holding_register);
    }
    return core::RecordField{std::move(record_array)};
  }

  [[nodiscard]] std::byte getFunctionCode() const override {
    switch (register_type_) {
      case RegisterType::holding:
        return std::byte{0x03};
      case RegisterType::input:
        return std::byte{0x04};
      default:
        throw std::invalid_argument(fmt::format("Invalid RegisterType {}", magic_enum::enum_underlying(register_type_)));
    }
  }

  bool operator==(const ReadModbusFunction& rhs) const override {
    const auto read_holding_registers_rhs = dynamic_cast<const ReadRegisters<T>*>(&rhs);
    if (!read_holding_registers_rhs)
      return false;

    return read_holding_registers_rhs->number_of_points_ == this->number_of_points_  &&
           read_holding_registers_rhs->starting_address_ == this->starting_address_ &&
           read_holding_registers_rhs->transaction_id_ == this->transaction_id_;
  }

 protected:
  RegisterType register_type_{};
  uint16_t starting_address_{};
  uint16_t number_of_points_{};
};

inline std::unique_ptr<ReadModbusFunction> parseReadRegister(const RegisterType register_type,
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

inline std::unique_ptr<ReadModbusFunction> ReadModbusFunction::parse(const uint16_t transaction_id, const uint8_t unit_id, const std::string& address) {
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

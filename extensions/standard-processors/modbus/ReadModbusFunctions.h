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

#include "minifi-cpp/core/RecordField.h"
#include "fmt/format.h"
#include "modbus/ByteConverters.h"
#include "modbus/Error.h"
#include "range/v3/algorithm/copy.hpp"
#include "range/v3/view/chunk.hpp"
#include "range/v3/view/drop.hpp"
#include "utils/StringUtils.h"
#include "utils/expected.h"

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

  [[nodiscard]] std::vector<std::byte> requestBytes() const;

  [[nodiscard]] uint16_t getTransactionId() const { return transaction_id_; }
  [[nodiscard]] uint8_t getUnitId() const { return unit_id_; }
  [[nodiscard]] virtual nonstd::expected<core::RecordField, std::error_code> responseToRecordField(std::span<const std::byte> resp_pdu) const = 0;

  static std::unique_ptr<ReadModbusFunction> parse(uint16_t transaction_id, uint8_t unit_id, const std::string& address);

 protected:
  [[nodiscard]] auto getRespBytes(std::span<const std::byte> resp_pdu) const -> nonstd::expected<std::span<const std::byte>, std::error_code>;

  [[nodiscard]] virtual std::byte getFunctionCode() const = 0;
  [[nodiscard]] virtual std::array<std::byte, 5> rawPdu() const = 0;
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

  [[nodiscard]] nonstd::expected<core::RecordField, std::error_code> responseToRecordField(std::span<const std::byte> resp_pdu) const override;

  [[nodiscard]] std::byte getFunctionCode() const override;
  [[nodiscard]] std::array<std::byte, 5> rawPdu() const override;
  [[nodiscard]] uint8_t expectedByteCount() const override;

  bool operator==(const ReadCoilStatus& rhs) const = default;
  bool operator==(const ReadModbusFunction& rhs) const override;

  static std::unique_ptr<ReadModbusFunction> parse(uint16_t transaction_id, uint8_t unit_id, std::string_view address_str, std::string_view length_str);

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

  [[nodiscard]] std::array<std::byte, 5> rawPdu() const override {
    std::array<std::byte, 5> result;
    result[0] = getFunctionCode();
    ranges::copy(toBytes(starting_address_), (result | ranges::views::drop(1) | ranges::views::take(2)).begin());
    ranges::copy(toBytes(wordCount()), (result | ranges::views::drop(3) | ranges::views::take(2)).begin());

    return result;
  }

  [[nodiscard]] uint8_t expectedByteCount() const override {
    return gsl::narrow<uint8_t>(number_of_points_*std::max(sizeof(T), sizeof(uint16_t)));
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

}  // namespace org::apache::nifi::minifi::modbus

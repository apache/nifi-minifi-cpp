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

#include <string>
#include <system_error>
#include "magic_enum.hpp"

namespace org::apache::nifi::minifi::modbus {

enum class ModbusExceptionCode : std::underlying_type_t<std::byte> {
  IllegalFunction = 0x01,
  IllegalDataAddress = 0x02,
  IllegalDataValue = 0x03,
  SlaveDeviceFailure = 0x04,
  Acknowledge = 0x05,
  SlaveDeviceBusy = 0x06,
  NegativeAcknowledge = 0x07,
  MemoryParityError = 0x08,
  GatewayPathUnavailable = 0x0a,
  GatewayTargetDeviceFailedToRespond = 0x0b,
  InvalidResponse,
  MessageTooLarge,
  InvalidTransactionId,
  IllegalProtocol,
  InvalidSlaveId
};


struct ModbusErrorCategory final : std::error_category {
  [[nodiscard]] const char* name() const noexcept override {
    return "modbus error";
  }

  [[nodiscard]] std::string message(int ev) const override {
    const auto modbus_exception_code = static_cast<ModbusExceptionCode>(ev);
    return std::string{magic_enum::enum_name<ModbusExceptionCode>(modbus_exception_code)};
  }
};

inline const ModbusErrorCategory& modbus_category() noexcept {
  static ModbusErrorCategory category;
  return category;
};

inline std::error_code make_error_code(ModbusExceptionCode c) {
  return {static_cast<int>(c), modbus_category()};
}

}  // namespace org::apache::nifi::minifi::modbus

template <>
struct std::is_error_code_enum<org::apache::nifi::minifi::modbus::ModbusExceptionCode> : true_type {};

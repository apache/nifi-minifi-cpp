// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <string>
#include <system_error>

#include "magic_enum/magic_enum.hpp"

namespace org::apache::nifi::minifi::modbus {

//  Based from https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf MODBUS Exception Codes
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
  InvalidSlaveId,
  MessageTooShort,
  UnexpectedResponseFunctionCode,
  UnexpectedResponsePDUSize
};

struct ModbusErrorCategory final : std::error_category {
  [[nodiscard]] const char* name() const noexcept override { return "Modbus Error"; }

  [[nodiscard]] std::string message(int ev) const override {
    const auto modbus_exception_code = static_cast<ModbusExceptionCode>(ev);
    auto modbus_exception_code_str = std::string{magic_enum::enum_name<ModbusExceptionCode>(modbus_exception_code)};
    if (modbus_exception_code_str.empty()) { return fmt::format("UNKNOWN ERROR {}", ev); }
    return modbus_exception_code_str;
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

template<>
struct std::is_error_code_enum<org::apache::nifi::minifi::modbus::ModbusExceptionCode> : std::true_type {};

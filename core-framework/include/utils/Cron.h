// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <exception>
#include <string>
#include <chrono>
#include <optional>
#include <memory>
#include <utility>
#include "date/tz.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::utils {
class BadCronExpression : public minifi::Exception {
 public:
  explicit BadCronExpression(const std::string& errmsg) : minifi::Exception(errmsg) {}
};

class CronField {
 public:
  virtual ~CronField() = default;

  [[nodiscard]] virtual bool matches(date::local_seconds time_point) const = 0;
  virtual bool operator==(const CronField&) const { throw std::runtime_error("not implemented"); }
};

class Cron {
 public:
  explicit Cron(const std::string& expression);

  [[nodiscard]] std::optional<date::local_seconds> calculateNextTrigger(date::local_seconds start) const;

  std::unique_ptr<CronField> second_;
  std::unique_ptr<CronField> minute_;
  std::unique_ptr<CronField> hour_;
  std::unique_ptr<CronField> day_;
  std::unique_ptr<CronField> month_;
  std::unique_ptr<CronField> day_of_week_;
  std::unique_ptr<CronField> year_;
};

}  // namespace org::apache::nifi::minifi::utils

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
#pragma once

#include <exception>
#include <string>
#include <chrono>
#include <optional>
#include <memory>
#include <utility>
#include "date/tz.h"

namespace org::apache::nifi::minifi::utils {
class BadCronExpression : public std::exception {
 public:
  explicit BadCronExpression(std::string msg) : msg_(std::move(msg)) {}

  [[nodiscard]] const char* what() const noexcept override { return (msg_.c_str()); }

 private:
  std::string msg_;
};

class CronField {
 public:
  virtual ~CronField() = default;

  [[nodiscard]] virtual bool isValid(date::local_seconds time_point) const = 0;
};

class Cron {
 public:
  explicit Cron(const std::string& expression);
  Cron(Cron&& cron) = default;

  [[nodiscard]] std::optional<date::local_seconds> calculateNextTrigger(date::local_seconds start) const;

  std::unique_ptr<CronField> second_, minute_, hour_, day_, month_, day_of_week_, year_;
};

}  // namespace org::apache::nifi::minifi::utils

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

#include "CpuStat.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

std::optional<CpuStatData> CpuStatData::parseCpuStatLine(std::istream& iss) {
  CpuStatData data;
  iss >> data.user_ >> data.nice_ >> data.system_ >> data.idle_ >> data.io_wait_ >> data.irq_ >> data.soft_irq_ >> data.steal_ >> data.guest_ >> data.guest_nice_;
  if (iss.fail())
    return std::nullopt;
  return data;
}

CpuStatData CpuStatData::operator-(const CpuStatData& rhs) const {
  CpuStatData result;
  result.user_ = user_ - rhs.user_;
  result.nice_ = nice_ - rhs.nice_;
  result.system_ = system_ - rhs.system_;
  result.idle_ = idle_ - rhs.idle_;
  result.io_wait_ = io_wait_ - rhs.io_wait_;
  result.irq_ = irq_ - rhs.irq_;
  result.soft_irq_ = soft_irq_ - rhs.soft_irq_;
  result.steal_ = steal_ - rhs.steal_;
  result.guest_ = guest_ - rhs.guest_;
  result.guest_nice_ = guest_nice_ - rhs.guest_nice_;
  return result;
}

}  // namespace org::apache::nifi::minifi::extensions::procfs

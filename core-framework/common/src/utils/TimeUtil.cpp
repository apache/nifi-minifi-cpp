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

#include "utils/TimeUtil.h"
#include "range/v3/algorithm/contains.hpp"
#include "fmt/format.h"
#include "fmt/chrono.h"

#ifdef WIN32
#include "date/tz.h"
#endif

namespace org::apache::nifi::minifi::utils::timeutils {

using namespace std::literals::chrono_literals;

namespace {
template<class... Durations>
std::tuple<Durations...> breakDownDurations(std::chrono::system_clock::duration input_duration) {
  std::tuple<Durations...> result;

  ([&]<typename T>(T& duration) {
    duration = std::chrono::duration_cast<std::decay_t<T>>(input_duration);
    input_duration -= duration;
  } (std::get<Durations>(result)), ...);

  return result;
}

std::string formatAsDaysHoursMinutesSeconds(std::chrono::system_clock::duration input_duration) {
  const auto durs = breakDownDurations<std::chrono::days, std::chrono::hours, std::chrono::minutes, std::chrono::seconds>(input_duration);
  const auto& days = std::get<std::chrono::days>(durs);
  std::string day_string;
  if (days.count() > 0) {
    day_string = fmt::format("{} {}", days.count(), days.count() == 1 ? "day, " : "days, ");
  }
  return fmt::format("{}{:02}:{:02}:{:02}",
                     day_string, std::get<std::chrono::hours>(durs).count(),
                     std::get<std::chrono::minutes>(durs).count(),
                     std::get<std::chrono::seconds>(durs).count());
}

template<class... Durations>
std::string formatAsRoundedLargestUnit(std::chrono::system_clock::duration input_duration) {
  std::optional<std::string> rounded_value_str;
  using std::chrono::duration_cast;
  using std::chrono::duration;

  ((rounded_value_str = input_duration >= Durations(1)
                        ? std::optional<std::string>{fmt::format("{:.2}", duration_cast<duration<double, typename Durations::period>>(input_duration))}
                        : std::nullopt) || ...);


  if (!rounded_value_str) {
    return fmt::format("{}", input_duration);
  }

  return *rounded_value_str;
}

}  // namespace

std::string humanReadableDuration(std::chrono::system_clock::duration input_duration) {
  if (input_duration > 5s) {
    return formatAsDaysHoursMinutesSeconds(input_duration);
  }

  return formatAsRoundedLargestUnit<std::chrono::seconds, std::chrono::milliseconds, std::chrono::microseconds>(input_duration);
}

std::optional<std::chrono::system_clock::time_point> parseRfc3339(const std::string& str) {
  std::istringstream stream(str);
  date::year_month_day date_part{};
  date::from_stream(stream, "%F", date_part);

  if (stream.fail())
    return std::nullopt;

  constexpr std::string_view accepted_delimiters = "tT_ ";
  char delimiter_char = 0;
  stream.get(delimiter_char);

  if (stream.fail() || !ranges::contains(accepted_delimiters, delimiter_char))
    return std::nullopt;

  std::chrono::system_clock::duration time_part;
  std::chrono::minutes offset = 0min;
  if (str.ends_with('Z') || str.ends_with('z')) {
    date::from_stream(stream, "%T", time_part);
    if (stream.fail())
      return std::nullopt;
    stream.get();
  } else {
    date::from_stream(stream, "%T%Ez", time_part, {}, &offset);
  }

  if (stream.fail() || (stream.peek() && !stream.eof()))
    return std::nullopt;

  return date::sys_days(date_part) + time_part - offset;
}

}  // namespace org::apache::nifi::minifi::utils::timeutils

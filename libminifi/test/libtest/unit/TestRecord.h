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
#include <numbers>

#include "minifi-cpp/core/Record.h"

namespace org::apache::nifi::minifi::core::test {

inline Record createSampleRecord2(const bool stringify = false) {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  Record record;

  constexpr auto when = date::sys_days(2022_y / 11 / 01) + 19h + 52min + 11s;
  if (!stringify) {
    record.emplace("when", RecordField{when});
  } else {
    record.emplace("when", RecordField{utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(when))});
  }
  record.emplace("foo", RecordField{std::string{"Lorem ipsum dolor sit amet, consectetur adipiscing elit."}});
  record.emplace("bar", RecordField{int64_t{98402134}});
  record.emplace("baz", RecordField{std::numbers::pi});
  record.emplace("is_test", RecordField{true});
  RecordArray qux;
  qux.emplace_back('x');
  qux.emplace_back('y');
  qux.emplace_back('z');

  RecordObject quux;
  quux["Apfel"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"pomme"})};
  quux["Birne"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"poire"})};
  quux["Aprikose"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"abricot"})};

  RecordArray corge;
  corge.emplace_back(false);
  corge.emplace_back(false);

  record.emplace("qux", RecordField{std::move(qux)});
  record.emplace("quux", RecordField{std::move(quux)});
  record.emplace("corge", RecordField{std::move(corge)});
  return record;
}

inline Record createSampleRecord(const bool stringify = false) {
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
  Record record;

  constexpr auto when = date::sys_days(2012_y / 07 / 01) + 9h + 53min + 00s;
  if (!stringify) {
    record.emplace("when", RecordField{when});
  } else {
    record.emplace("when", RecordField{utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(when))});
  }
  record.emplace("foo", RecordField{std::string{"asd"}});
  record.emplace("bar", RecordField{int64_t{123}});
  record.emplace("baz", RecordField{3.14});
  record.emplace("is_test", RecordField{true});
  RecordArray qux;
  qux.emplace_back('a');
  qux.emplace_back('b');
  qux.emplace_back('c');
  RecordObject quux;
  quux["Apfel"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"apple"})};
  quux["Birne"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"pear"})};
  quux["Aprikose"] = BoxedRecordField{std::make_unique<RecordField>(std::string{"apricot"})};

  RecordArray corge;
  corge.emplace_back(true);
  corge.emplace_back(false);

  record.emplace("qux", RecordField{std::move(qux)});
  record.emplace("quux", RecordField{std::move(quux)});
  record.emplace("corge", RecordField{std::move(corge)});
  return record;
}

}  // namespace org::apache::nifi::minifi::core::test

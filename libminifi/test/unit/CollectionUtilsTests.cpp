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

#include <string>
#include <vector>
#include <set>
#include "utils/CollectionUtils.h"
#include "../TestBase.h"
#include "../Catch.h"

TEST_CASE("TestHaveCommonItem", "[haveCommonItem]") {
  auto verify = [](std::initializer_list<std::string> a, std::initializer_list<std::string> b, bool result) {
    std::vector<std::string> v1{a};
    std::set<std::string> s1{a};
    std::vector<std::string> v2{b};
    std::set<std::string> s2{b};
    REQUIRE(utils::haveCommonItem(v1, v2) == result);
    REQUIRE(utils::haveCommonItem(v1, s2) == result);
    REQUIRE(utils::haveCommonItem(s1, v2) == result);
    REQUIRE(utils::haveCommonItem(s1, s2) == result);
  };

  verify({"a"}, {"a"}, true);
  verify({"a"}, {"a", "b"}, true);
  verify({"a"}, {}, false);
  verify({}, {"a"}, false);
  verify({"a"}, {"b"}, false);
  verify({}, {}, false);
}

template<typename T>
struct MockSet : std::set<T> {
  using std::set<T>::set;
  [[nodiscard]] auto find(const T& item) const -> decltype(this->std::set<T>::find(item)) {
    on_find_();
    return std::set<T>::find(item);
  }
  std::function<void()> on_find_;
};

TEST_CASE("TestHaveCommonItem set::find is called", "[haveCommonItem set::find]") {
  MockSet<std::string> s1{"a"};
  bool member_find_called = false;
  s1.on_find_ = [&] {member_find_called = true;};

  REQUIRE(utils::haveCommonItem(std::set<std::string>{"a"}, s1));
  REQUIRE(member_find_called);
}

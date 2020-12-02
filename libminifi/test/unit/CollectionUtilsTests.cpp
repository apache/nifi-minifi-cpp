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

TEST_CASE("TestHaveCommonItem", "[haveCommonItem]") {
  using Vals = std::initializer_list<std::string>;
  struct TestCase {
    Vals lhs;
    Vals rhs;
    bool result;
  };
  std::vector<TestCase> cases{
    {Vals{"a"}, Vals{"a"}, true},
    {Vals{"a"}, Vals{"a", "b"}, true},
    {Vals{"a"}, Vals{}, false},
    {Vals{}, Vals{"a"}, false},
    {Vals{"a"}, Vals{"b"}, false},
    {Vals{}, Vals{}, false}
  };

  for (const auto& test_case : cases) {
    std::vector<std::string> v1{test_case.lhs};
    std::set<std::string> s1{test_case.lhs};
    std::vector<std::string> v2{test_case.rhs};
    std::set<std::string> s2{test_case.rhs};
    REQUIRE(utils::haveCommonItem(v1, v2) == test_case.result);
    REQUIRE(utils::haveCommonItem(v1, s2) == test_case.result);
    REQUIRE(utils::haveCommonItem(s1, v2) == test_case.result);
    REQUIRE(utils::haveCommonItem(s1, s2) == test_case.result);
  }
}

template<typename T>
struct MockSet : std::set<T> {
  using std::set<T>::set;
  auto find(const T& item) const -> decltype(std::set<T>::find(item)) {
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

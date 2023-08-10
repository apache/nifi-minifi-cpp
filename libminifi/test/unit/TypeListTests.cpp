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

#include "../Catch.h"
#include "core/Core.h"
#include "utils/meta/type_list.h"

class A {};

namespace outer {
class B {};
class C {};

namespace inner {
class D {};
}
}

template<typename... Types>
using type_list = org::apache::nifi::minifi::utils::meta::type_list<Types...>;

TEST_CASE("an empty type_list doesn't contain anything") {
  STATIC_CHECK_FALSE(type_list<>::contains<A>());
  STATIC_CHECK_FALSE(type_list<>::contains<outer::B>());
  STATIC_CHECK_FALSE(type_list<>::contains<outer::inner::D>());
  STATIC_CHECK_FALSE(type_list<>::contains<std::string>());
}

TEST_CASE("a non-empty type_list contains what it should") {
  STATIC_CHECK(type_list<A, outer::B>::contains<A>());
  STATIC_CHECK(type_list<A, outer::B>::contains<outer::B>());
  STATIC_CHECK_FALSE(type_list<A, outer::B>::contains<outer::C>());
  STATIC_CHECK_FALSE(type_list<A, outer::B>::contains<outer::inner::D>());

  STATIC_CHECK(type_list<int, A, std::string>::contains<int>());
  STATIC_CHECK(type_list<int, A, std::string>::contains<A>());
  STATIC_CHECK(type_list<int, A, std::string>::contains<std::string>());
  STATIC_CHECK_FALSE(type_list<int, A, std::string>::contains<double>());
  STATIC_CHECK_FALSE(type_list<int, A, std::string>::contains<outer::C>());
}

TEST_CASE("toStrings() on a type_list produces the correct list of types") {
  STATIC_CHECK(type_list<>::AsStringViews.empty());
  STATIC_CHECK(type_list<A>::AsStringViews == std::array<std::string_view, 1>{"A"});
  STATIC_CHECK(type_list<A, outer::B>::AsStringViews == std::array<std::string_view, 2>{"A", "outer::B"});
  STATIC_CHECK(type_list<A, outer::C, outer::inner::D>::AsStringViews == std::array<std::string_view, 3>{"A", "outer::C", "outer::inner::D"});
  STATIC_CHECK(type_list<outer::C, A, outer::inner::D, outer::B>::AsStringViews == std::array<std::string_view, 4>{"outer::C", "A", "outer::inner::D", "outer::B"});
  STATIC_CHECK(type_list<double, A, std::vector<int>>::AsStringViews == std::array<std::string_view, 3>{"double", "A", org::apache::nifi::minifi::core::className<std::vector<int>>()});
}

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/Catch.h"
#include "core/Core.h"
#include "utils/meta/type_list.h"

class A {};

namespace outer {
class B {};
class C {};

namespace inner {
class C {};
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

  namespace inner = outer::inner;
  STATIC_CHECK(type_list<outer::C>::contains<outer::C>());
  STATIC_CHECK(type_list<inner::C>::contains<inner::C>());
  STATIC_CHECK_FALSE(type_list<outer::C>::contains<inner::C>());
  STATIC_CHECK_FALSE(type_list<inner::C>::contains<outer::C>());
}

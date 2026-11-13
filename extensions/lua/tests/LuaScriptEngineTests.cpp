// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "LuaScriptEngine.h"

namespace org::apache::nifi::minifi::extensions::lua::test {

TEST_CASE("LuaScriptEngine errors during eval", "[luascriptengineeval]") {
  LuaScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval("print('foo')"));
  // The exception message comes from the lua engine
  REQUIRE_THROWS_MATCHES(
    engine.eval("shout('foo')"),
    LuaScriptException,
    minifi::test::utils::ExceptionSubStringMatcher<LuaScriptException>({"global 'shout' is not callable (a nil value)", "attempt to call a nil value", "attempt to call global 'shout'"}));
}

TEST_CASE("LuaScriptEngine errors during call", "[luascriptenginecall]") {
  LuaScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval(R"(
    function foo()
      print('foo')
    end

    function bar()
      shout('bar')
    end
  )"));
  REQUIRE_NOTHROW(engine.call("foo"));
  // The exception message comes from the lua engine
  REQUIRE_THROWS_MATCHES(
    engine.call("bar"),
    LuaScriptException,
    minifi::test::utils::ExceptionSubStringMatcher<LuaScriptException>({"global 'shout' is not callable (a nil value)", "attempt to call a nil value", "attempt to call global 'shout'"}));
}

}  // namespace org::apache::nifi::minifi::extensions::lua::test

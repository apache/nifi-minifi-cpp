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

#include "TestBase.h"
#include "Catch.h"
#include "Utils.h"
#include "lua/LuaScriptEngine.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("LuaScriptEngine errors during eval", "[luascriptengineeval]") {
  lua::LuaScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval("print('foo')"));
  // The exception message comes from the lua engine
  REQUIRE_THROWS_MATCHES(
    engine.eval("shout('foo')"),
    script::ScriptException,
    utils::ExceptionSubStringMatcher<script::ScriptException>({"global 'shout' is not callable (a nil value)", "attempt to call a nil value", "attempt to call global 'shout'"}));
}

TEST_CASE("LuaScriptEngine errors during call", "[luascriptenginecall]") {
  lua::LuaScriptEngine engine;
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
    script::ScriptException,
    utils::ExceptionSubStringMatcher<script::ScriptException>({"global 'shout' is not callable (a nil value)", "attempt to call a nil value", "attempt to call global 'shout'"}));
}

}  // namespace org::apache::nifi::minifi::test

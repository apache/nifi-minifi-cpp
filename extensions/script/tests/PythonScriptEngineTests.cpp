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
#include "python/PythonScriptEngine.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("PythonScriptEngine errors during eval", "[pythonscriptengineeval]") {
  python::PythonScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval("print('foo')"));
  REQUIRE_THROWS_MATCHES(engine.eval("shout('foo')"), script::ScriptException, utils::ExceptionSubStringMatcher<script::ScriptException>({"name 'shout' is not defined"}));
}

TEST_CASE("PythonScriptEngine errors during call", "[luascriptenginecall]") {
  python::PythonScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval(R"(
    def foo():
      print('foo')

    def bar():
      shout('bar')

  )"));
  REQUIRE_NOTHROW(engine.call("foo"));
  REQUIRE_THROWS_MATCHES(engine.call("bar"), script::ScriptException, utils::ExceptionSubStringMatcher<script::ScriptException>({"name 'shout' is not defined"}));
}

}  // namespace org::apache::nifi::minifi::test

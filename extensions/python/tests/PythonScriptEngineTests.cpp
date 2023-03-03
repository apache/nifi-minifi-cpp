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

#include <barrier>
#include "TestBase.h"
#include "Catch.h"
#include "Utils.h"
#include "PythonScriptEngine.h"

namespace org::apache::nifi::minifi::extensions::python {

using minifi::test::utils::ExceptionSubStringMatcher;

TEST_CASE("PythonScriptEngine errors during eval", "[pythonscriptengineeval]") {
  python::PythonScriptEngine engine;
  REQUIRE_NOTHROW(engine.eval("print('foo')"));
  REQUIRE_THROWS_MATCHES(engine.eval("shout('foo')"), PythonScriptException, ExceptionSubStringMatcher<PythonScriptException>({"NameError: name 'shout' is not defined"}));
  REQUIRE_THROWS_MATCHES(engine.eval(" print('foo')"), PythonScriptException, ExceptionSubStringMatcher<PythonScriptException>({"IndentationError: unexpected indent (<string>, line 2)"}));
}

TEST_CASE("GilScopedAcquire should lock threads properly", "[pythonscriptengineeval]") {
  python::PythonScriptEngine engine;

  for (int i = 0; i < 10; ++i) {
    DYNAMIC_SECTION("Iteration: " << i) {
      std::vector<std::string_view> messages;
      std::barrier sync{2};
      auto sleeping_thread = std::thread([&] {
        using namespace std::literals::chrono_literals;
        python::GlobalInterpreterLock gil_lock;
        std::ignore = sync.arrive();
        messages.emplace_back("Before sleep");
        std::this_thread::sleep_for(100ms);
        messages.emplace_back("First thread");
      });
      sync.arrive_and_wait();

      auto non_sleeping_thread = std::thread([&] {
        python::GlobalInterpreterLock gil_lock;
        messages.emplace_back("Second thread");
      });
      non_sleeping_thread.join();
      sleeping_thread.join();
      REQUIRE(messages == std::vector<std::string_view>{"Before sleep", "First thread", "Second thread"});
    }
  }
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
  REQUIRE_THROWS_MATCHES(engine.call("bar"), PythonScriptException, ExceptionSubStringMatcher<PythonScriptException>({"name 'shout' is not defined"}));
}

}  // namespace org::apache::nifi::minifi::extensions::python

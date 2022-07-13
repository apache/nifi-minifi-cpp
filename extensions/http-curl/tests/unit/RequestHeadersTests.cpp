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

#include "RequestHeaders.h"
#include "Catch.h"

namespace org::apache::nifi::minifi::extensions::curl::testing {

bool containsHeaderString(const curl_slist* const headers, std::string_view string_to_contain) {
  const curl_slist* curr_node = headers;
  while (curr_node) {
    const std::string_view curr_data = curr_node->data;
    if (curr_data == string_to_contain)
      return true;
    curr_node = curr_node->next;
  }
  return false;
}

TEST_CASE("AppendHeader", "curl::RequestHeaders") {
  RequestHeaders headers;
  headers.appendHeader("foo", "bar");
  auto curl_headers = headers.get();
  REQUIRE(curl_headers);
  CHECK(containsHeaderString(curl_headers.get(), "foo: bar"));
}

}  // namespace org::apache::nifi::minifi::extensions::curl::testing

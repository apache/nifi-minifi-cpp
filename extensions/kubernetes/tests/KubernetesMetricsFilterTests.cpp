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
#include "Catch.h"

#include "ContainerInfo.h"
#include "MetricsFilter.h"
#include "utils/StringUtils.h"

namespace minifi = org::apache::nifi::minifi;

namespace {
constexpr const char* RESULT_HEADER = R"({"kind":"PodMetricsList","apiVersion":"metrics.k8s.io/v1beta1","metadata":{},"items":[)";

constexpr const char* HELLO_WORLD_POD_HEADER = R"({"metadata":{"name":"hello-world","namespace":"default","creationTimestamp":"2022-07-15T14:08:36Z"},)"
    R"("timestamp":"2022-07-15T14:08:35Z","window":"30.817s","containers":[)";

constexpr const char* HELLO_WORLD_CONTAINER_ONE = R"({"name":"echo-one","usage":{"cpu":"123n","memory":"1596Ki"}})";

constexpr const char* HELLO_WORLD_CONTAINER_TWO = R"({"name":"echo-two","usage":{"cpu":"456n","memory":"1560Ki"}})";

constexpr const char* MINIFI_POD = R"({"metadata":{"name":"minifi","namespace":"default","creationTimestamp":"2022-07-15T14:08:36Z"},)"
    R"("timestamp":"2022-07-15T14:08:22Z","window":"6.342s",)"
    R"("containers":[{"name":"minifi","usage":{"cpu":"924956n","memory":"8800Ki"}}]})";

constexpr const char* SYSTEM_POD = R"({"metadata":{"name":"kube-apiserver-kind-control-plane","namespace":"kube-system","creationTimestamp":"2022-07-15T14:08:36Z",)"
    R"("labels":{"component":"kube-apiserver","tier":"control-plane"}},"timestamp":"2022-07-15T14:08:29Z","window":"18.516s",)"
    R"("containers":[{"name":"kube-apiserver","usage":{"cpu":"83095152n","memory":"270396Ki"}}]})";

const std::string API_RESULT = minifi::utils::StringUtils::join_pack(
    RESULT_HEADER, HELLO_WORLD_POD_HEADER, HELLO_WORLD_CONTAINER_ONE, ",", HELLO_WORLD_CONTAINER_TWO, "]},", MINIFI_POD, ",", SYSTEM_POD, "]}");

void testSuccessfulFiltering(const std::function<bool(const minifi::kubernetes::ContainerInfo&)>& filter, const std::string& expected_result) {
  const auto filtered_result = minifi::kubernetes::metrics::filter(API_RESULT, filter);
  REQUIRE(filtered_result.has_value());
  CHECK(filtered_result.value() == expected_result);
}

const auto ALWAYS_TRUE = [](const minifi::kubernetes::ContainerInfo&) { return true; };
}  // namespace

TEST_CASE("Without filtering, we get the full result back", "[kubernetes][metrics_filter]") {
  testSuccessfulFiltering(ALWAYS_TRUE, API_RESULT);
}

TEST_CASE("We can filter to get metrics for the default namespace only", "[kubernetes][metrics_filter]") {
  const auto default_namespace_only = [](const minifi::kubernetes::ContainerInfo& container_info) { return container_info.name_space == "default"; };
  const auto expected_result = minifi::utils::StringUtils::join_pack(
      RESULT_HEADER, HELLO_WORLD_POD_HEADER, HELLO_WORLD_CONTAINER_ONE, ",", HELLO_WORLD_CONTAINER_TWO, "]},", MINIFI_POD, "]}");
  testSuccessfulFiltering(default_namespace_only, expected_result);
}

TEST_CASE("We can filter to get metrics for a selected pod only", "[kubernetes][metrics_filter]") {
  const auto minifi_pod_only = [](const minifi::kubernetes::ContainerInfo& container_info) { return container_info.pod_name == "minifi"; };
  const auto expected_result = minifi::utils::StringUtils::join_pack(RESULT_HEADER, MINIFI_POD, "]}");
  testSuccessfulFiltering(minifi_pod_only, expected_result);
}

TEST_CASE("We can filter to get metrics for a selected container only", "[kubernetes][metrics_filter]") {
  const auto one_container_only = [](const minifi::kubernetes::ContainerInfo& container_info) { return container_info.pod_name == "hello-world" && container_info.container_name == "echo-two"; };
  const auto expected_result = minifi::utils::StringUtils::join_pack(RESULT_HEADER, HELLO_WORLD_POD_HEADER, HELLO_WORLD_CONTAINER_TWO, "]}]}");
  testSuccessfulFiltering(one_container_only, expected_result);
}

TEST_CASE("We can filter to get no metrics (not super useful)", "[kubernetes][metrics_filter]") {
  const auto always_false = [](const minifi::kubernetes::ContainerInfo&) { return false; };
  const auto expected_result = minifi::utils::StringUtils::join_pack(RESULT_HEADER, "]}");
  testSuccessfulFiltering(always_false, expected_result);
}

TEST_CASE("An invalid JSON will return an error", "[kubernetes][metrics_filter]") {
  const auto filtered_result = minifi::kubernetes::metrics::filter(R"({"foo":1, bar:2})", ALWAYS_TRUE);
  REQUIRE_FALSE(filtered_result.has_value());
  CHECK(filtered_result.error() == "Error parsing the metrics received from the Kubernetes API at offset 10: Missing a name for object member.");
}

TEST_CASE("A JSON with an unexpected structure will return an error", "[kubernetes][metrics_filter]") {
  const auto filtered_result = minifi::kubernetes::metrics::filter(R"({"foo":1, "bar":2})", ALWAYS_TRUE);
  REQUIRE_FALSE(filtered_result.has_value());
  CHECK(filtered_result.error() == "Unexpected JSON received from the Kubernetes API: missing list of 'items'");
}

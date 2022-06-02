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

#include "../PutElasticsearchJson.h"
#include "../ElasticsearchCredentialsControllerService.h"
#include "MockElastic.h"
#include "SingleProcessorTestController.h"
#include "Catch.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch::test {

TEST_CASE("PutElasticsearchJson", "[elastic]") {
  MockElastic mock_elastic("10433");

  std::shared_ptr<PutElasticsearchJson> put_elasticsearch_json = std::make_shared<PutElasticsearchJson>("PutElasticsearchJson");
  minifi::test::SingleProcessorTestController test_controller{put_elasticsearch_json};
  auto elasticsearch_credentials_controller_service = test_controller.plan->addController("ElasticsearchCredentialsControllerService", "elasticsearch_credentials_controller_service");
  CHECK(test_controller.plan->setProperty(put_elasticsearch_json,
                                     PutElasticsearchJson::ElasticCredentials.getName(),
                                     "elasticsearch_credentials_controller_service"));
  CHECK(test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::Hosts.getName(),
                                    "localhost:10433"));
  CHECK(test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::IndexOperation.getName(),
                                    "${elastic_operation}"));
  CHECK(test_controller.plan->setProperty(put_elasticsearch_json,
                                    PutElasticsearchJson::Index.getName(),
                                    "test_index"));

  SECTION("Index with valid basic authentication") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::AuthorizationType.getName(),
                                            toString(ElasticsearchCredentialsControllerService::AuthType::USE_BASIC_AUTHENTICATION)));

    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Username.getName(),
                                            MockElasticAuthHandler::USERNAME));
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Password.getName(),
                                            MockElasticAuthHandler::PASSWORD));

    std::vector<std::tuple<const std::string_view, std::unordered_map<std::string, std::string>>> inputs;

    auto results = test_controller.trigger({std::make_tuple<const std::string_view, std::unordered_map<std::string, std::string>>(R"({"field1":"value1"}")", {{"elastic_operation", "index"}}),
                                            std::make_tuple<const std::string_view, std::unordered_map<std::string, std::string>>(R"({"field1":"value2"}")", {{"elastic_operation", "index"}})});
    REQUIRE(results[PutElasticsearchJson::Success].size() == 2);
    for (const auto& result : results[PutElasticsearchJson::Success]) {
      auto attributes = result->getAttributes();
      CHECK(attributes.contains("elasticsearch.index._id"));
      CHECK(attributes.contains("elasticsearch.index._index"));
    }
  }

  SECTION("Create with valid ApiKey") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::AuthorizationType.getName(),
                                            toString(ElasticsearchCredentialsControllerService::AuthType::USE_API_KEY)));

    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::ApiKey.getName(),
                                            MockElasticAuthHandler::API_KEY));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_operation", "create"}});
    REQUIRE(results[PutElasticsearchJson::Success].size() == 1);
    auto attributes = results[PutElasticsearchJson::Success][0]->getAttributes();
    CHECK(attributes.contains("elasticsearch.create._id"));
    CHECK(attributes.contains("elasticsearch.create._index"));
  }

  SECTION("Invalid ApiKey") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::AuthorizationType.getName(),
                                            toString(ElasticsearchCredentialsControllerService::AuthType::USE_API_KEY)));

    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::ApiKey.getName(),
                                            "invalid_api_key"));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_operation", "create"}});
    CHECK(results[PutElasticsearchJson::Failure].size() == 1);
  }

  SECTION("Invalid basic authentication") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::AuthorizationType.getName(),
                                            toString(ElasticsearchCredentialsControllerService::AuthType::USE_BASIC_AUTHENTICATION)));

    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Username.getName(),
                                            MockElasticAuthHandler::USERNAME));
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Password.getName(),
                                            "wrong_password"));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_operation", "index"}});
    CHECK(results[PutElasticsearchJson::Failure].size() == 1);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch::test

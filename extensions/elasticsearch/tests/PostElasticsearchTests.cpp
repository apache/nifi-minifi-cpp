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

#include "../PostElasticsearch.h"
#include "../ElasticsearchCredentialsControllerService.h"
#include "MockElastic.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/Catch.h"
#include "core/FlowFile.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch::test {

TEST_CASE("PostElasticsearch", "[elastic]") {
  MockElastic mock_elastic("10433");

  auto post_elasticsearch_json = std::make_shared<PostElasticsearch>("PostElasticsearch");
  minifi::test::SingleProcessorTestController test_controller{post_elasticsearch_json};
  auto elasticsearch_credentials_controller_service = test_controller.plan->addController("ElasticsearchCredentialsControllerService", "elasticsearch_credentials_controller_service");
  CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                     PostElasticsearch::ElasticCredentials,
                                     "elasticsearch_credentials_controller_service"));
  CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                    PostElasticsearch::Hosts,
                                    "localhost:10433"));
  CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                    PostElasticsearch::Action,
                                    "${elastic_action}"));
  CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                    PostElasticsearch::Index,
                                    "test_index"));

  SECTION("Index with valid basic authentication") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Username,
                                            MockElasticAuthHandler::USERNAME));
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Password,
                                            MockElasticAuthHandler::PASSWORD));

    auto results = test_controller.trigger({{R"({"field1":"value1"}")", {{"elastic_action", "index"}}},
                                            {R"({"field1":"value2"}")", {{"elastic_action", "index"}}}});
    REQUIRE(results[PostElasticsearch::Success].size() == 2);
    for (const auto& result : results[PostElasticsearch::Success]) {
      auto attributes = result->getAttributes();
      CHECK(attributes.contains("elasticsearch.index._id"));
      CHECK(attributes.contains("elasticsearch.index._index"));
    }
  }

  SECTION("Update with valid ApiKey") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::ApiKey,
                                            MockElasticAuthHandler::API_KEY));
    CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                            PostElasticsearch::Identifier,
                                            "${filename}"));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_action", "upsert"}});
    REQUIRE(results[PostElasticsearch::Success].size() == 1);
    auto attributes = results[PostElasticsearch::Success][0]->getAttributes();
    CHECK(attributes.contains("elasticsearch.update._id"));
    CHECK(attributes.contains("elasticsearch.update._index"));
  }

  SECTION("Update error") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::ApiKey,
                                            MockElasticAuthHandler::API_KEY));
    CHECK(test_controller.plan->setProperty(post_elasticsearch_json,
                                            PostElasticsearch::Identifier,
                                            "${filename}"));
    mock_elastic.returnErrors(true);
    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_action", "upsert"}});
    REQUIRE(results[PostElasticsearch::Error].size() == 1);
    auto attributes = results[PostElasticsearch::Error][0]->getAttributes();
    CHECK(attributes.contains("elasticsearch.update._id"));
    CHECK(attributes.contains("elasticsearch.update._index"));
    CHECK(attributes.contains("elasticsearch.update.error.type"));
    CHECK(attributes.contains("elasticsearch.update.error.reason"));
  }

  SECTION("Invalid ApiKey") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::ApiKey,
                                            "invalid_api_key"));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_action", "create"}});
    CHECK(results[PostElasticsearch::Failure].size() == 1);
  }

  SECTION("Invalid basic authentication") {
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Username,
                                            MockElasticAuthHandler::USERNAME));
    CHECK(test_controller.plan->setProperty(elasticsearch_credentials_controller_service,
                                            ElasticsearchCredentialsControllerService::Password,
                                            "wrong_password"));

    auto results = test_controller.trigger(R"({"field1":"value1"}")", {{"elastic_action", "index"}});
    CHECK(results[PostElasticsearch::Failure].size() == 1);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch::test

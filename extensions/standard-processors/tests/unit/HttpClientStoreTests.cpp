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
#include <thread>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "InvokeHTTP.h"
#include "http/BaseHTTPClient.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class HttpClientStoreTestAccessor {
 public:
  static const std::vector<std::pair<std::unique_ptr<minifi::http::HTTPClient>, bool>>& getClients(minifi::processors::invoke_http::HttpClientStore& store) {
    return store.clients_;
  }

  static size_t getMaxSize(minifi::processors::invoke_http::HttpClientStore& store) {
    return store.max_size_;
  }

  static size_t getClientsCreated(minifi::processors::invoke_http::HttpClientStore& store) {
    return store.clients_created_;
  }
};

TEST_CASE("HttpClientStore can create new client for a url and is returned after it's not used anymore") {
  minifi::processors::invoke_http::HttpClientStore store(2, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  REQUIRE(HttpClientStoreTestAccessor::getMaxSize(store) == 2);
  REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 0);
  {
    [[maybe_unused]] auto client = store.getClient("http://localhost:8080");
    const auto& clients = HttpClientStoreTestAccessor::getClients(store);
    REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 1);
    REQUIRE(clients[0].second == true);
  }

  const auto& clients = HttpClientStoreTestAccessor::getClients(store);
  REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 1);
  REQUIRE(clients[0].second == false);
}

TEST_CASE("A http client can be reused") {
  minifi::processors::invoke_http::HttpClientStore store(2, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  minifi::http::HTTPClient* client_ptr = nullptr;
  auto& clients = HttpClientStoreTestAccessor::getClients(store);
  {
    auto client = store.getClient("http://localhost:8080");
    client_ptr = &client.get();
  }

  {
    auto client = store.getClient("http://localhost:8080");
    REQUIRE(&client.get() == client_ptr);
    REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 1);
    REQUIRE(clients[0].second);
  }
  REQUIRE_FALSE(clients[0].second);
}

TEST_CASE("A new url always creates a new client") {
  minifi::processors::invoke_http::HttpClientStore store(3, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  auto& clients = HttpClientStoreTestAccessor::getClients(store);
  {
    [[maybe_unused]] auto client1 = store.getClient("http://localhost:8080");
    [[maybe_unused]] auto client2 = store.getClient("http://localhost:8081");
    [[maybe_unused]] auto client3 = store.getClient("http://localhost:8082");
    REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 3);
    REQUIRE(clients[0].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[1].first->getURL() == "http://localhost:8081");
    REQUIRE(clients[2].first->getURL() == "http://localhost:8082");
  }
  REQUIRE(clients[0].first->getURL() == "http://localhost:8082");
  REQUIRE(clients[1].first->getURL() == "http://localhost:8081");
  REQUIRE(clients[2].first->getURL() == "http://localhost:8080");
}

TEST_CASE("If a store is full, the first unused client is replaced by the newly requested one") {
  minifi::processors::invoke_http::HttpClientStore store(3, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  auto& clients = HttpClientStoreTestAccessor::getClients(store);
  {
    [[maybe_unused]] auto client1 = store.getClient("http://localhost:8080");
    {
      [[maybe_unused]] auto client2 = store.getClient("http://localhost:8081");
    }
    [[maybe_unused]] auto client3 = store.getClient("http://localhost:8082");
    REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 3);
    REQUIRE(clients[0].first->getURL() == "http://localhost:8081");
    REQUIRE_FALSE(clients[0].second);
    REQUIRE(clients[1].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[1].second);
    REQUIRE(clients[2].first->getURL() == "http://localhost:8082");
    REQUIRE(clients[2].second);

    [[maybe_unused]] auto client4 = store.getClient("http://localhost:8083");
    REQUIRE(clients[0].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[0].second);
    REQUIRE(clients[1].first->getURL() == "http://localhost:8082");
    REQUIRE(clients[1].second);
    REQUIRE(clients[2].first->getURL() == "http://localhost:8083");
    REQUIRE(clients[2].second);
  }
  REQUIRE(clients[0].first->getURL() == "http://localhost:8083");
  REQUIRE_FALSE(clients[0].second);
  REQUIRE(clients[1].first->getURL() == "http://localhost:8082");
  REQUIRE_FALSE(clients[1].second);
  REQUIRE(clients[2].first->getURL() == "http://localhost:8080");
  REQUIRE_FALSE(clients[2].second);
}

TEST_CASE("Multiple unused clients are present the oldest one is replaced") {
  minifi::processors::invoke_http::HttpClientStore store(4, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  auto& clients = HttpClientStoreTestAccessor::getClients(store);
  {
    [[maybe_unused]] auto client1 = store.getClient("http://localhost:8080");
    {
      [[maybe_unused]] auto client2 = store.getClient("http://localhost:8081");
      [[maybe_unused]] auto client3 = store.getClient("http://localhost:8082");
    }
    [[maybe_unused]] auto client4 = store.getClient("http://localhost:8083");
    REQUIRE(HttpClientStoreTestAccessor::getClientsCreated(store) == 4);
    REQUIRE(clients[0].first->getURL() == "http://localhost:8082");
    REQUIRE_FALSE(clients[0].second);
    REQUIRE(clients[1].first->getURL() == "http://localhost:8081");
    REQUIRE_FALSE(clients[1].second);
    REQUIRE(clients[2].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[2].second);
    REQUIRE(clients[3].first->getURL() == "http://localhost:8083");
    REQUIRE(clients[3].second);

    [[maybe_unused]] auto client5 = store.getClient("http://localhost:8084");
    REQUIRE(clients[0].first->getURL() == "http://localhost:8081");
    REQUIRE_FALSE(clients[0].second);
    REQUIRE(clients[1].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[1].second);
    REQUIRE(clients[2].first->getURL() == "http://localhost:8083");
    REQUIRE(clients[2].second);
    REQUIRE(clients[3].first->getURL() == "http://localhost:8084");
    REQUIRE(clients[3].second);
  }
  REQUIRE(clients[0].first->getURL() == "http://localhost:8081");
  REQUIRE_FALSE(clients[0].second);
  REQUIRE(clients[1].first->getURL() == "http://localhost:8084");
  REQUIRE_FALSE(clients[1].second);
  REQUIRE(clients[2].first->getURL() == "http://localhost:8083");
  REQUIRE_FALSE(clients[2].second);
  REQUIRE(clients[3].first->getURL() == "http://localhost:8080");
  REQUIRE_FALSE(clients[3].second);
}

TEST_CASE("If all clients are in use, the call will block until a client is returned") {
  minifi::processors::invoke_http::HttpClientStore store(2, [](const std::string& url) {
    auto client = std::make_unique<minifi::http::HTTPClient>();
    client->initialize(minifi::http::HttpRequestMethod::GET, url, nullptr);
    return gsl::make_not_null(std::move(client));
  });
  bool client2_created{false};
  std::mutex mutex;
  std::condition_variable client2_created_cv;
  [[maybe_unused]] auto client1 = store.getClient("http://localhost:8080");

  std::thread thread1([&store, &mutex, &client2_created, &client2_created_cv] {
    std::unique_lock lock(mutex);
    [[maybe_unused]] auto client2 = store.getClient("http://localhost:8081");
    client2_created = true;
    lock.unlock();
    client2_created_cv.notify_one();
    std::this_thread::sleep_for(300ms);
  });

  std::thread thread2([&store, &mutex, &client2_created, &client2_created_cv] {
    std::unique_lock lock(mutex);
    client2_created_cv.wait(lock, [&client2_created] { return client2_created; });
    [[maybe_unused]] auto client3 = store.getClient("http://localhost:8082");
    auto& clients = HttpClientStoreTestAccessor::getClients(store);
    REQUIRE(clients[0].first->getURL() == "http://localhost:8080");
    REQUIRE(clients[0].second);
    REQUIRE(clients[1].first->getURL() == "http://localhost:8082");
    REQUIRE(clients[1].second);
  });

  thread1.join();
  thread2.join();
}

}  // namespace org::apache::nifi::minifi::test

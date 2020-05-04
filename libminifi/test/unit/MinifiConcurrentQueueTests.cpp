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

#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <set>

#include "../TestBase.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/StringUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

namespace {

namespace MinifiConcurrentQueueTestProducersConsumers {

  // Producers

  template <typename Queue>
  std::thread getSimpleProducerThread(Queue& queue) {
    return std::thread([&queue] {
      queue.enqueue("ba");
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      queue.enqueue("dum");
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      queue.enqueue("tss");
    });
  }

  std::thread getBlockedProducerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::mutex& mutex) {
    return std::thread([&queue, &mutex] {
      std::unique_lock<std::mutex> lock(mutex);
      queue.enqueue("ba");
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      queue.enqueue("dum");
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      queue.enqueue("tss");
    });
  }

  // Consumers

  std::thread getSimpleTryDequeConsumerThread(utils::ConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (queue.tryDequeue(s)) {
          results.push_back(s);
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getSimpleConsumeConsumerThread(utils::ConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (!queue.consume([&results] (const std::string& s) { results.push_back(s); })) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getDequeueWaitConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      std::string s;
      while (queue.dequeueWait(s)) {
        results.push_back(s);
      }
    });
  }

  std::thread getConsumeWaitConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (!queue.consumeWait([&results] (const std::string& s) { results.push_back(s); })) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getSpinningReaddingDequeueConsumerThread(utils::ConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (queue.tryDequeue(s)) {
          // Unique elements only
          if (!std::count(results.begin(), results.end(), s)) {
            results.push_back(s);
          }
          queue.enqueue(std::move(s));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getReaddingDequeueConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      std::string s;
      while (queue.dequeueWait(s)) {
        if (!std::count(results.begin(), results.end(), s)) {
          results.push_back(s);
        }
        // The consumer is busy enqueing so noone is waiting for this ;(
        queue.enqueue(std::move(s));
      }
    });
  }

  std::thread getDequeueWaitForConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results] {
      const std::size_t max_read_attempts = 6;
      for (std::size_t attempt_num = 0; results.size() < 3 && attempt_num < max_read_attempts; ++attempt_num) {
        std::string s;
        if (queue.dequeueWaitFor(s, std::chrono::milliseconds(3))) {
          results.push_back(s);
        }
      }
    });
  }

  std::thread getConsumeWaitForConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::vector<std::string>& results) {
    return std::thread([&queue, &results]() {
      const std::size_t max_read_attempts = 6;
      std::size_t attempt_num = 0;
      while (results.size() < 3 && attempt_num < max_read_attempts) {
        ++attempt_num;
        std::string s;
        queue.consumeWaitFor([&results] (const std::string& s) { results.push_back(s); }, std::chrono::milliseconds(3));
      }
    });
  }

}  // namespace MinifiConcurrentQueueTestProducersConsumers

TEST_CASE("TestConcurrentQueue", "[TestConcurrentQueue]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;

  utils::ConcurrentQueue<std::string> queue;
  std::vector<std::string> results;

  SECTION("empty queue") {
    SECTION("default initialized queue is empty") {
      REQUIRE(queue.empty());
    }

    SECTION("trying to update based on empty queue preserves original data") {
      std::string s { "Unchanged" };

      SECTION("tryDequeue on empty queue returns false") {
        REQUIRE(!queue.tryDequeue(s));
      }

      SECTION("consume on empty queue returns false") {
        bool ret = queue.consume([&s] (const std::string& elem) { s = elem; });
        REQUIRE(!ret);
      }
      REQUIRE(s == "Unchanged");
    }
  }

  SECTION("non-empty queue") {
    SECTION("the queue is first-in-first-out") {
      for (std::size_t i = 0; i < 20; ++i) {
        queue.enqueue(std::to_string(i));
      }
      SECTION("tryDequeue preserves order") {
        for (std::size_t i = 0; i < 20; ++i) {
          std::string s;
          queue.tryDequeue(s);
          REQUIRE(s == std::to_string(i));
        }
        REQUIRE(queue.empty());
      }
      SECTION("consume preserves order") {
        for (std::size_t i = 0; i < 20; ++i) {
          std::string s;
          queue.consume([&s] (const std::string& elem) { s = elem; });
          REQUIRE(s == std::to_string(i));
        }
        REQUIRE(queue.empty());
      }
      SECTION("insertion does not reorder") {
        for (std::size_t i = 0; i < 20; ++i) {
          std::string s;
          queue.tryDequeue(s);
          queue.enqueue("0");
          queue.enqueue("9");
          REQUIRE(s == std::to_string(i));
        }
        REQUIRE(40 == queue.size());
      }
    }
  }
}

TEST_CASE("TestConcurrentQueue::testProducerConsumer", "[TestConcurrentQueueProducerConsumer]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConcurrentQueue<std::string> queue;
  std::vector<std::string> results;

  SECTION("producers and consumers work synchronized") {
    std::thread producer;
    std::thread consumer;
    SECTION("using tryDequeue") {
        producer = getSimpleProducerThread(queue);
        consumer = getSimpleTryDequeConsumerThread(queue, results);
      }
    SECTION("using consume") {
        producer = getSimpleProducerThread(queue);
        consumer = getSimpleConsumeConsumerThread(queue, results);
    }
    /* In this testcase the consumer thread puts back all items to the queue to consume again
    * Even in this case the ones inserted later by the producer should be consumed */
    SECTION("with readd") {
      producer = getSimpleProducerThread(queue);
      consumer = getSpinningReaddingDequeueConsumerThread(queue, results);
    }
    producer.join();
    consumer.join();
  }

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

TEST_CASE("TestConditionConcurrentQueue::testProducerConsumer", "[TestConditionConcurrentQueueProducerConsumer]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::vector<std::string> results;

  SECTION("consumers fetching data from producers is synchronized and fifo") {
    std::thread producer { getSimpleProducerThread(queue) };
    std::thread consumer;
    SECTION("using dequeueWait") {
      consumer = getDequeueWaitConsumerThread(queue, results);
    }
    SECTION("using consumeWait") {
      consumer = getConsumeWaitConsumerThread(queue, results);
    }
    SECTION("using dequeueWaitFor") {
      consumer = getDequeueWaitForConsumerThread(queue, results);
    }
    SECTION("using consumeWaitFor") {
      consumer = getConsumeWaitForConsumerThread(queue, results);
    }
    producer.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    queue.stop();
    consumer.join();

    REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
  }

  /* The same test as above, but covering the ConditionConcurrentQueue */
  SECTION("with readd") {
    std::thread consumer { getReaddingDequeueConsumerThread(queue, results) };
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::thread producer { getSimpleProducerThread(queue) };
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    producer.join();
    queue.stop();
    consumer.join();

    REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
  }

  // Blocked producers
  SECTION("consumer times out when using waitFor and time is up") {
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    std::thread producer{ getBlockedProducerThread(queue, mutex) };
    std::thread consumer;
    SECTION("using dequeueWaitFor") {
      consumer = getDequeueWaitForConsumerThread(queue, results);
    }
    SECTION("using consumeWaitFor") {
      consumer = getConsumeWaitForConsumerThread(queue, results);
    }
    consumer.join();
    lock.unlock();
    producer.join();

    REQUIRE(results.empty());
  }
}

TEST_CASE("TestConcurrentQueues::highLoad", "[TestConcurrentQueuesHighLoad]") {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(1, std::numeric_limits<int>::max());

  std::vector<int> source(1000000);
  std::vector<int> target;

  generate(source.begin(), source.end(), [&rng, &dist](){ return dist(rng); });

  utils::ConcurrentQueue<int> queue;
  utils::ConditionConcurrentQueue<int> cqueue(true);

  std::thread producer([&source, &queue]()  {
    for (int i : source) { queue.enqueue(i); }
  });

  std::thread relay([&queue, &cqueue]() {
    size_t cnt = 0;
    while (cnt < 1000000) {
      int i;
      if (queue.tryDequeue(i)) {
        cnt++;
        cqueue.enqueue(i);
      }
    }
  });

  std::thread consumer([&cqueue, &target]() {
    int i;
    while (cqueue.dequeueWait(i)) {
      target.push_back(i);
    }
  });

  producer.join();
  relay.join();

  while (cqueue.size() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(0));
  }

  cqueue.stop();
  consumer.join();

  REQUIRE(source == target);
}
}  // anon namespace

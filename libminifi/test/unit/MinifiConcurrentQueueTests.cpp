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

  std::thread getReaddingDequeueConsumerThread(utils::ConcurrentQueue<std::string>& queue, std::set<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (queue.tryDequeue(s)) {
          results.insert(s);
          queue.enqueue(std::move(s));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getInfiniteReaddingDequeueConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::set<std::string>& results) {
    return std::thread([&queue, &results] {
      while (results.size() < 3) {
        std::string s;
        if (queue.tryDequeue(s)) {
          results.insert(s);
          queue.enqueue(std::move(s));
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });
  }

  std::thread getDequeueWaitForConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::set<std::string>& results) {
    return std::thread([&queue, &results] {
      const std::size_t max_read_attempts = 6;
      std::size_t attemt_num = 0;
      while (results.size() < 3 && attemt_num < max_read_attempts) {
        ++attemt_num;
        std::string s;
        if (queue.dequeueWaitFor(s, std::chrono::milliseconds(3))) {
          results.insert(s);
        }
      }
    });
  }

  std::thread getConsumeWaitForConsumerThread(utils::ConditionConcurrentQueue<std::string>& queue, std::set<std::string>& results) {
    return std::thread([&queue, &results]() {
      const std::size_t max_read_attempts = 6;
      std::size_t attemt_num = 0;
      while (results.size() < 3 && attemt_num < max_read_attempts) {
        ++attemt_num;
        std::string s;
        queue.consumeWaitFor([&results] (const std::string& s) { results.insert(s); }, std::chrono::milliseconds(3));
      }
    });
  }

}  // namespace MinifiConcurrentQueueTestProducersConsumers

TEST_CASE("TestConcurrentQueue::testQueue", "[TestQueue]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConcurrentQueue<std::string> queue;
  std::vector<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getSimpleTryDequeConsumerThread(queue, results)};
  producer.join();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

TEST_CASE("TestConditionConcurrentQueue::testQueueUsingApply", "[TestConditionQueueUsingApply]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConcurrentQueue<std::string> queue;
  std::vector<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getSimpleConsumeConsumerThread(queue, results) };

  producer.join();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}


TEST_CASE("TestConditionConcurrentQueue::testConditionQueue", "[TestConditionQueue]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::vector<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getDequeueWaitConsumerThread(queue, results)};
  producer.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  queue.stop();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}


TEST_CASE("TestConditionConcurrentQueue::testConditionQueueConsumeWait", "[TestConditionQueueApplyWait]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::vector<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getConsumeWaitConsumerThread(queue, results) };

  producer.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  queue.stop();
  consumer.join();
  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

/* In this testcase the consumer thread puts back all items to the queue to consume again
 * Even in this case the ones inserted later by the producer  should be consumed */
TEST_CASE("TestConcurrentQueue::testQueueWithReAdd", "[TestQueueWithSingleReAdd]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConcurrentQueue<std::string> queue;
  std::set<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getReaddingDequeueConsumerThread(queue, results) };

  producer.join();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

/* The same test as above, but covering the ConditionConcurrentQueue */
TEST_CASE("TestConditionConcurrentQueue::testConditionQueueWithReAdd", "[TestConditionQueueWithInfReAdd]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::set<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getInfiniteReaddingDequeueConsumerThread(queue, results) };
  producer.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  queue.stop();
  consumer.join();
  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

TEST_CASE("TestConditionConcurrentQueue::testConditionQueueDequeueWaitForWithSignal", "[testConditionQueueDequeueWaitForWithSignal]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::set<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getDequeueWaitForConsumerThread(queue, results) };
  producer.join();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

TEST_CASE("TestConditionConcurrentQueue::testConditionQueueConsumeWaitForWithSignal", "[testConditionQueueconsumeWaitForWithSignal]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::set<std::string> results;

  std::thread producer{ getSimpleProducerThread(queue) };
  std::thread consumer{ getConsumeWaitForConsumerThread(queue, results) };
  producer.join();
  consumer.join();

  REQUIRE(utils::StringUtils::join("-", results) == "ba-dum-tss");
}

TEST_CASE("TestConditionConcurrentQueue::testConditionQueueDequeueWaitForNoSignal", "[testConditionQueueDequeueWaitForNoSignal]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::set<std::string> results;
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);

  std::thread producer{ getBlockedProducerThread(queue, mutex) };
  std::thread consumer{ getDequeueWaitForConsumerThread(queue, results) };
  consumer.join();
  lock.unlock();
  producer.join();

  REQUIRE(0 == results.size());
}

TEST_CASE("TestConditionConcurrentQueue::testConditionQueueconsumeWaitForNoSignal", "[testConditionQueueconsumeWaitForNoSignal]") {
  using namespace MinifiConcurrentQueueTestProducersConsumers;
  utils::ConditionConcurrentQueue<std::string> queue(true);
  std::set<std::string> results;
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);

  std::thread producer{ getBlockedProducerThread(queue, mutex) };
  std::thread consumer{ getConsumeWaitForConsumerThread(queue, results) };
  consumer.join();
  lock.unlock();
  producer.join();

  REQUIRE(0 == results.size());
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

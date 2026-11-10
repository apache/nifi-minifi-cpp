// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>

#include "utils/StringUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/StagingQueue.h"

using org::apache::nifi::minifi::utils::StagingQueue;

class MockItem {
 public:
  static MockItem allocate(size_t max_size) {
    MockItem instance;
    instance.data_.reserve(max_size * 3 / 2);
    return instance;
  }

  MockItem commit() {
    return std::move(*this);
  }

  [[nodiscard]] size_t size() const {
    return data_.size();
  }

  std::string data_;
};

TEST_CASE("Construct queue", "[TestStagingQueue1]") {
  StagingQueue<MockItem> queue(30, 10);
  REQUIRE(queue.size() == 0);
}

TEST_CASE("Modify no commit", "[TestStagingQueue2]") {
  StagingQueue<MockItem> queue(30, 10);
  queue.modify([] (MockItem& item) {
    item.data_ += "12345";
  });
  REQUIRE(queue.size() == 5);
  SECTION("Decrease size") {
    queue.modify([] (MockItem& item) {
      REQUIRE(item.data_ == "12345");
      item.data_ = "";
    });
    REQUIRE(queue.size() == 0);
  }
  MockItem out;
  REQUIRE(!queue.tryDequeue(out));
}

TEST_CASE("Modify and commit", "[TestStagingQueue3]") {
  StagingQueue<MockItem> queue(30, 10);
  queue.modify([] (MockItem& item) {
    item.data_ += "12345";
  });
  queue.commit();
  SECTION("Commit is idempotent if there is no modification between") {
    queue.commit();
  }
  REQUIRE(queue.size() == 5);
  MockItem out;
  REQUIRE(queue.tryDequeue(out));
  REQUIRE(out.data_ == "12345");
  REQUIRE(queue.size() == 0);
}

TEST_CASE("Modify and overflow triggered automatic commit", "[TestStagingQueue4]") {
  StagingQueue<MockItem> queue(30, 10);
  queue.modify([] (MockItem& item) {
    item.data_ += "123456789ab";
  });
  SECTION("Explicit commit makes no difference") {
    queue.commit();
  }
  queue.modify([] (MockItem& item) {
    // a new item has been allocated
    REQUIRE(item.data_.empty());
  });
  REQUIRE(queue.size() == 11);
  MockItem out;
  REQUIRE(queue.tryDequeue(out));
  REQUIRE(out.data_ == "123456789ab");
  REQUIRE(queue.size() == 0);
}

TEST_CASE("Discard overflow", "[TestStagingQueue5]") {
  StagingQueue<MockItem> queue(30, 10);
  for (size_t idx = 0; idx < 5; ++idx) {
    queue.modify([&] (MockItem& item) {
      item.data_ = utils::string::repeat(std::to_string(idx), 10);
    });
    queue.commit();
  }
  REQUIRE(queue.size() == 50);
  queue.discardOverflow();
  REQUIRE(queue.size() == 30);
  MockItem out;
  // idx 0 and 1 have been discarded
  for (size_t idx = 2; idx < 5; ++idx) {
    REQUIRE(queue.tryDequeue(out));
    REQUIRE(out.data_ == utils::string::repeat(std::to_string(idx), 10));
  }
  REQUIRE(queue.size() == 0);
}

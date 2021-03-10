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
#pragma once

#include <memory>
#include <queue>
#include <vector>

#include "core/FlowFile.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class FlowFileQueue {
 public:
  using value_type = std::shared_ptr<core::FlowFile>;

  value_type pop();
  void push(const value_type& element);
  void push(value_type&& element);
  bool isWorkAvailable() const;
  bool empty() const;
  size_t size() const;

 private:
  struct FlowFilePenaltyExpirationComparator {
    bool operator()(const value_type& left, const value_type& right);
  };

  std::priority_queue<value_type, std::vector<value_type>, FlowFilePenaltyExpirationComparator> queue_;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

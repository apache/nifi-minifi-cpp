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

#pragma once

#include <vector>

#include <soci/soci.h>

#include "SQLRowSubscriber.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

class SQLRowsetProcessor {
 public:
  SQLRowsetProcessor(const soci::rowset<soci::row>& rowset, std::vector<std::reference_wrapper<SQLRowSubscriber>> rowSubscribers);

  size_t process(size_t max);

 private:
   void addRow(const soci::row& row, size_t rowCount);

   template <typename T>
   void processColumn(const std::string& name, const T& value) const {
     for (const auto& subscriber: row_subscribers_) {
       subscriber.get().processColumn(name, value);
     }
   }

 private:
  soci::rowset<soci::row>::const_iterator iter_;
  soci::rowset<soci::row> rowset_;
  std::vector<std::reference_wrapper<SQLRowSubscriber>> row_subscribers_;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


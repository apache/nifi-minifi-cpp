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

#include "SQLRowsetProcessor.h"

#include "Exception.h"
#include "Utils.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

SQLRowsetProcessor::SQLRowsetProcessor(const soci::rowset<soci::row>& rowset, std::vector<std::reference_wrapper<SQLRowSubscriber>> row_subscribers)
  : rowset_(rowset), row_subscribers_(std::move(row_subscribers)) {
  iter_ = rowset_.begin();
}

size_t SQLRowsetProcessor::process(size_t max) {
  size_t count = 0;

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().beginProcessBatch();
  }

  for (; iter_ != rowset_.end(); ) {
    addRow(*iter_, count);
    iter_++;
    count++;
    if (max > 0 && count >= max) {
      break;
    }
  }

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().endProcessBatch(count == 0 ? SQLRowSubscriber::Progress::DONE : SQLRowSubscriber::Progress::CONTINUE);
  }

  return count;
}

void SQLRowsetProcessor::addRow(const soci::row& row, size_t rowCount) {
  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().beginProcessRow();
  }

  if (rowCount == 0) {
    std::vector<std::string> column_names;
    column_names.reserve(row.size());
    for (std::size_t i = 0; i != row.size(); ++i) {
      column_names.push_back(utils::StringUtils::toLower(row.get_properties(i).get_name()));
    }
    for (const auto& subscriber : row_subscribers_) {
      subscriber.get().processColumnNames(column_names);
    }
  }

  for (std::size_t i = 0; i != row.size(); ++i) {
    const soci::column_properties& props = row.get_properties(i);

    const auto& name = utils::StringUtils::toLower(props.get_name());

    if (row.get_indicator(i) == soci::i_null) {
      processColumn(name, "NULL");
    } else {
      switch (const auto dataType = props.get_data_type()) {
        case soci::data_type::dt_string: {
          processColumn(name, row.get<std::string>(i));
        }
        break;
        case soci::data_type::dt_double: {
          processColumn(name, row.get<double>(i));
        }
        break;
        case soci::data_type::dt_integer: {
          processColumn(name, row.get<int>(i));
        }
        break;
        case soci::data_type::dt_long_long: {
          processColumn(name, row.get<long long>(i));
        }
        break;
        case soci::data_type::dt_unsigned_long_long: {
          processColumn(name, row.get<unsigned long long>(i));
        }
        break;
        case soci::data_type::dt_date: {
          const std::tm when = row.get<std::tm>(i);

          char value[128];
          if (!std::strftime(value, sizeof(value), "%Y-%m-%d %H:%M:%S", &when))
            throw minifi::Exception(PROCESSOR_EXCEPTION, "SQLRowsetProcessor: !strftime.");

          processColumn(name, std::string(value));
        }
        break;
        default: {
          throw minifi::Exception(PROCESSOR_EXCEPTION, "SQLRowsetProcessor: Unsupported data type " + std::to_string(dataType));
        }
      }
    }
  }

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().endProcessRow();
  }
}

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


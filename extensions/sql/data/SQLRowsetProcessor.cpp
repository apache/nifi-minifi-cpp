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

#include "minifi-cpp/Exception.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::sql {

SQLRowsetProcessor::SQLRowsetProcessor(std::unique_ptr<Rowset> rowset, std::vector<std::reference_wrapper<SQLRowSubscriber>> row_subscribers)
  : rowset_(std::move(rowset)), row_subscribers_(std::move(row_subscribers)) {
  rowset_->reset();
}

size_t SQLRowsetProcessor::process(size_t max) {
  size_t count = 0;

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().beginProcessBatch();
  }

  while (!rowset_->is_done()) {
    addRow(rowset_->getCurrent(), count);
    rowset_->next();
    count++;
    if (max > 0 && count >= max) {
      break;
    }
  }

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().endProcessBatch();
    if (count == 0) {
      subscriber.get().finishProcessing();
    }
  }

  return count;
}

void SQLRowsetProcessor::addRow(const Row& row, size_t rowCount) {
  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().beginProcessRow();
  }

  if (rowCount == 0) {
    std::vector<std::string> column_names;
    column_names.reserve(row.size());
    for (std::size_t i = 0; i != row.size(); ++i) {
      column_names.push_back(row.getColumnName(i));
    }
    for (const auto& subscriber : row_subscribers_) {
      subscriber.get().processColumnNames(column_names);
    }
  }

  for (std::size_t i = 0; i != row.size(); ++i) {
    const auto& name = row.getColumnName(i);

    if (row.isNull(i)) {
      processColumn(name, "NULL");
    } else {
      switch (row.getDataType(i)) {
        case DataType::STRING: {
          processColumn(name, row.getString(i));
        }
        break;
        case DataType::DOUBLE: {
          processColumn(name, row.getDouble(i));
        }
        break;
        case DataType::INTEGER: {
          processColumn(name, row.getInteger(i));
        }
        break;
        case DataType::LONG_LONG: {
          processColumn(name, row.getLongLong(i));
        }
        break;
        case DataType::UNSIGNED_LONG_LONG: {
          processColumn(name, row.getUnsignedLongLong(i));
        }
        break;
        case DataType::DATE: {
          const std::tm when = row.getDate(i);

          std::array<char, 128> value{};
          if (!std::strftime(value.data(), value.size(), "%Y-%m-%d %H:%M:%S", &when))
            throw minifi::Exception(PROCESSOR_EXCEPTION, "SQLRowsetProcessor: !strftime.");

          processColumn(name, std::string(value.data()));
        }
        break;
        default: {
          throw minifi::Exception(PROCESSOR_EXCEPTION, "SQLRowsetProcessor: Unsupported data type in column");
        }
      }
    }
  }

  for (const auto& subscriber : row_subscribers_) {
    subscriber.get().endProcessRow();
  }
}

}  // namespace org::apache::nifi::minifi::sql

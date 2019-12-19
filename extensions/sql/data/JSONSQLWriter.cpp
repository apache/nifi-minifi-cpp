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

#include "JSONSQLWriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "Exception.h"
#include "Utils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

JSONSQLWriter::JSONSQLWriter(const soci::rowset<soci::row> &rowset, std::ostream *out, MaxCollector* pMaxCollector)
  : SQLWriter(rowset), json_payload_(rapidjson::kArrayType), output_stream_(out), pMaxCollector_(pMaxCollector) {
}

JSONSQLWriter::~JSONSQLWriter() {
}

bool JSONSQLWriter::addRow(const soci::row &row, size_t rowCount) {
  rapidjson::Document::AllocatorType &alloc = json_payload_.GetAllocator();
  rapidjson::Value rowobj(rapidjson::kObjectType);

  // 'countColumnsInMaxCollector' is used to check that all columns in maxCollector are in row columns.
  // It is checked here since don't know if it is possible in 'soci' to get coulmns info of a select statements without executing query.
  size_t countColumnsInMaxCollector = 0;

  for (std::size_t i = 0; i != row.size(); ++i) {
    const soci::column_properties & props = row.get_properties(i);

    const auto& columnName = utils::toLower(props.get_name());

    if (pMaxCollector_ && rowCount == 0 && pMaxCollector_->hasColumn(columnName)) {
      countColumnsInMaxCollector++;
    }

    rapidjson::Value name;
    name.SetString(props.get_name().c_str(), props.get_name().length(), alloc);

    rapidjson::Value valueVal;

    if (row.get_indicator(i) == soci::i_null) {
      const std::string null = "NULL";
      valueVal.SetString(null.c_str(), null.length(), alloc);
    } else {
      switch (const auto dataType = props.get_data_type()) {
        case soci::data_type::dt_string: {
          const auto value = std::string(row.get<std::string>(i));
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, '\'' + value + '\'');
          }
          valueVal.SetString(value.c_str(), value.length(), alloc);
        }
        break;
        case soci::data_type::dt_double: {
          const auto value = row.get<double>(i);
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, value);
          }
          valueVal.SetDouble(value);
        }
        break;
        case soci::data_type::dt_integer: {
          const auto value = row.get<int>(i);
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, value);
          }
          valueVal.SetInt(value);
        }
        break;
        case soci::data_type::dt_long_long: {
          const auto value = row.get<long long>(i);
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, value);
          }
          valueVal.SetInt64(value);
        }
        break;
        case soci::data_type::dt_unsigned_long_long: {
          const auto value = row.get<unsigned long long>(i);
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, value);
          }
          valueVal.SetUint64(value);
        }
        break;
        case soci::data_type::dt_date: {
          // It looks like design bug in soci - for `dt_date`, it returns std::tm, which doesn't have mmilliseconds, but DB 'datetime' has milliseconds.
          // Don't know if it is possible to get a string representation for 'dt_date' type.
          // The problem for maxCollector, if milliseconds value is not stored as a maximum, then when running query with 'select ... datetimeColumn > maxValue', 
          // it will be always at least one record since DB has milliseconds "maxValue.milliseconds".
          // For a workaround in the string representation for 'dt_date', add '999' for maxCollector (won't work for cases where time precision is important).
          const std::tm when = row.get<std::tm>(i);

          char strWhen[128];
          if (!std::strftime(strWhen, sizeof(strWhen), "%Y-%m-%d %H:%M:%S", &when))
            throw minifi::Exception(PROCESSOR_EXCEPTION, std::string("ExecuteSQL. !strftime with '") + strWhen + "'");

          const std::string value = strWhen;
          if (pMaxCollector_) {
            pMaxCollector_->updateMaxValue(columnName, '\'' + value + ".999'");
          }
          valueVal.SetString(value.c_str(), value.length(), alloc);
        }
        break;
        default: {
          throw minifi::Exception(PROCESSOR_EXCEPTION, "ExecuteSQL. Unsupported data type " + std::to_string(dataType));
        }
      }
    }

    rowobj.AddMember(name, valueVal, alloc);
  }

  if (pMaxCollector_ && rowCount == 0) {
    pMaxCollector_->checkNumberProcessedColumns(countColumnsInMaxCollector);
  }

  json_payload_.PushBack(rowobj, alloc);

  return true;
}

void JSONSQLWriter::write() {
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter < rapidjson::StringBuffer > writer(buffer);
	json_payload_.Accept(writer);
	*output_stream_ << buffer.GetString();
	json_payload_ = rapidjson::Document(rapidjson::kArrayType);
}

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

JSONSQLWriter::JSONSQLWriter(soci::rowset<soci::row> &rowset, std::ostream *out)
    : SQLWriter(rowset), json_payload(rapidjson::kArrayType), output_stream(out){
}

JSONSQLWriter::~JSONSQLWriter() {
}

bool JSONSQLWriter::addRow(soci::row &row) {
  rapidjson::Document::AllocatorType &alloc = json_payload.GetAllocator();
  rapidjson::Value rowobj(rapidjson::kObjectType);
  for (std::size_t i = 0; i != row.size(); ++i) {
    const soci::column_properties & props = row.get_properties(i);

    rapidjson::Value name;
    name.SetString(props.get_name().c_str(), props.get_name().length(), alloc);

    rapidjson::Value valueVal;
    switch (props.get_data_type()) {
      case soci::data_type::dt_string: {
        std::string str = std::string(row.get<std::string>(i));
        valueVal.SetString(str.c_str(), str.length(), alloc);
      }
        break;
      case soci::data_type::dt_double:
        valueVal.SetDouble(row.get<double>(i));
        break;
      case soci::data_type::dt_integer:
        valueVal.SetInt(row.get<int>(i));
        break;
      case soci::data_type::dt_long_long: {
        int64_t i64val = row.get<long long>(i);
        valueVal.SetInt64(i64val);
      }
        break;
      case soci::data_type::dt_unsigned_long_long: {
        uint64_t u64val = row.get<unsigned long long>(i);
        valueVal.SetUint64(u64val);
      }
        break;
      case soci::data_type::dt_date: {
        std::tm when = row.get<std::tm>(i);
        std::string str = std::string(asctime(&when));
        valueVal.SetString(str.c_str(), str.length(), alloc);
      }
        break;
    }

    rowobj.AddMember(name, valueVal, alloc);

  }
  json_payload.PushBack(rowobj, alloc);
  return true;
}

void JSONSQLWriter::write() {
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter < rapidjson::StringBuffer > writer(buffer);
	json_payload.Accept(writer);
	*output_stream << buffer.GetString();
	json_payload = rapidjson::Document(rapidjson::kArrayType);
}

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


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

#ifndef EXTENSIONS_SQL_DATA_JSONSQLWRITER_H_
#define EXTENSIONS_SQL_DATA_JSONSQLWRITER_H_

#include "SQLWriter.h"
#include "rapidjson/document.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

class JSONSQLWriter : public SQLWriter {
 public:
  explicit JSONSQLWriter(const soci::rowset<soci::row> &rowset, std::ostream *out);
  virtual ~JSONSQLWriter();

  bool addRow(const soci::row &set) override;

  void write() override;

 private:

  std::ostream *output_stream;
  rapidjson::Document json_payload;

};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_SQL_DATA_JSONSQLWRITER_H_ */

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

#ifndef EXTENSIONS_SQL_DATA_SQLWRITER_H_
#define EXTENSIONS_SQL_DATA_SQLWRITER_H_

#include "DatabaseConnectors.h"
#include <iostream>

#include <soci/soci.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

class SQLWriter {
 public:
  explicit SQLWriter(const soci::rowset<soci::row> &rowset);
  virtual ~SQLWriter();

  virtual size_t serialize(size_t max = 0) {
    size_t count = 0;

    for (; iter_ != rowset_.end(); ) {
      addRow(*iter_, count);
	    iter_++;
	    count++; 
	    total_count_++;
	    if (max > 0 && count >= max) {
		    break;
	    }
    }

    return count;
  }

  virtual bool addRow(const soci::row &set, size_t rowCount) = 0;

  virtual void write() = 0;

 private:

  size_t total_count_;

  soci::rowset<soci::row>::const_iterator iter_;

  soci::rowset<soci::row> rowset_;
};

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_SQL_DATA_SQLWRITER_H_ */

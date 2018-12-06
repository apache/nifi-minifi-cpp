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

#ifndef NIFI_MINIFI_CPP_EXECUTESQL_H
#define NIFI_MINIFI_CPP_EXECUTESQL_H

#include <core/Resource.h>
#include <core/Processor.h>

#include <concurrentqueue.h>

#include "SQLiteConnection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class ExecuteSQL : public core::Processor {
 public:
  explicit ExecuteSQL(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ExecuteSQL>::getLogger()) {
  }

  static core::Property ConnectionURL;
  static core::Property SQLStatement;

  static core::Relationship Success;
  static core::Relationship Original;
  static core::Relationship Failure;

  void initialize() override;
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override {
    logger_->log_error("onTrigger invocation with raw pointers is not implemented");
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session) override;

  class SQLReadCallback : public InputStreamCallback {
   public:
    explicit SQLReadCallback(std::shared_ptr<std::string> sql)
        : sql_(std::move(sql)) {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) override;

   private:
    std::shared_ptr<std::string> sql_;
  };

 private:
  std::shared_ptr<logging::Logger> logger_;
  moodycamel::ConcurrentQueue<std::shared_ptr<minifi::sqlite::SQLiteConnection>> conn_q_;

  std::string db_url_;
  std::string sql_;
};

REGISTER_RESOURCE(ExecuteSQL, "Execute provided SQL query. Query result rows will be outputted as new flow files with attribute keys equal to "
    "result column names and values equal to result values. There will be one output FlowFile per result row. This processor can be scheduled to "
    "run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. If it is triggered by an incoming FlowFile, "
    "then attributes of that FlowFile will be available when evaluating the query."); // NOLINT

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_EXECUTESQL_H

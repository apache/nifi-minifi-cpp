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
#include "core/ClassLoader.h"
#include "processors/ExecuteSQL.h"
#include "processors/PutSQL.h"
#include "processors/QueryDatabaseTable.h"
#include "services/ODBCConnector.h"
#include "utils/GeneralUtils.h"

class SQLFactory : public core::ObjectFactory {
 public:
  SQLFactory() = default;

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  std::string getName() override {
    return "SQLFactory";
  }

  std::string getClassName() override{
    return "SQLFactory";
  }
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  std::vector<std::string> getClassNames() override{
    return {"ExecuteSQL", "PutSQL", "QueryDatabaseTable", "ODBCService"};
  }

  template <typename T>
  static std::unique_ptr<ObjectFactory> getObjectFactory() {
    return utils::make_unique<core::DefautObjectFactory<T>>();
  }

  std::unique_ptr<ObjectFactory> assign(const std::string &class_name) override {
    if (utils::StringUtils::equalsIgnoreCase(class_name, "ExecuteSQL")) {
      return getObjectFactory<minifi::processors::ExecuteSQL>();
    }
    if (utils::StringUtils::equalsIgnoreCase(class_name, "PutSQL")) {
      return getObjectFactory<minifi::processors::PutSQL>();
    }
    if (utils::StringUtils::equalsIgnoreCase(class_name, "QueryDatabaseTable")) {
      return getObjectFactory<minifi::processors::QueryDatabaseTable>();
    }
    if (utils::StringUtils::equalsIgnoreCase(class_name, "ODBCService")) {
      return getObjectFactory<minifi::sql::controllers::ODBCService>();
    }

    return nullptr;
  }
};

extern "C" {
  DLL_EXPORT void *createSQLFactory(void);
}

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

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"

#include "DatabaseService.h"
#include "core/Resource.h"
#include "data/SociConnectors.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {
namespace controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide Database services
 */
class ODBCService : public DatabaseService {
 public:
  explicit ODBCService(const std::string &name, const utils::Identifier &uuid = {})
    : DatabaseService(name, uuid),
      logger_(logging::LoggerFactory<ODBCService>::getLogger()) {
    initialize();
  }

  explicit ODBCService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : DatabaseService(name),
        logger_(logging::LoggerFactory<ODBCService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  std::unique_ptr<sql::Connection> getConnection() const override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

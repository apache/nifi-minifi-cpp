/**
 * @file ProcessSessionFactory.h
 * ProcessSessionFactory class declaration
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
#ifndef LIBMINIFI_INCLUDE_CORE_PROCESSSESSIONFACTORY_H_
#define LIBMINIFI_INCLUDE_CORE_PROCESSSESSIONFACTORY_H_

#include <memory>

#include "ProcessContext.h"
#include "ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessSessionFactory Class
class ProcessSessionFactory {
 public:
  // Constructor
  /*!
   * Create a new process session factory
   */
  explicit ProcessSessionFactory(std::shared_ptr<ProcessContext> processContext)
      : process_context_(processContext) {
  }

  // Create the session
  virtual std::shared_ptr<ProcessSession> createSession();

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProcessSessionFactory(const ProcessSessionFactory &parent) = delete;
  ProcessSessionFactory &operator=(const ProcessSessionFactory &parent) = delete;

  virtual ~ProcessSessionFactory() = default;

 private:
  // ProcessContext
  std::shared_ptr<ProcessContext> process_context_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CORE_PROCESSSESSIONFACTORY_H_

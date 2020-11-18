/**
 * @file AWSInitializer.cpp
 * AWSInitializer class implementation
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

#include "AWSInitializer.h"

#include <memory>

#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/core/utils/memory/stl/AWSString.h"
#include "aws/core/utils/logging/AWSLogging.h"
#include "aws/core/platform/Environment.h"
#include "AWSSdkLogger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace utils {

AWSInitializer& AWSInitializer::get() {
  static AWSInitializer instance;
  return instance;
}

AWSInitializer::~AWSInitializer() {
  Aws::Utils::Logging::ShutdownAWSLogging();
  Aws::ShutdownAPI(options_);
}

AWSInitializer::AWSInitializer() {
  Aws::InitAPI(options_);
  Aws::Utils::Logging::InitializeAWSLogging(
      std::make_shared<AWSSdkLogger>());
}

}  // namespace utils
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

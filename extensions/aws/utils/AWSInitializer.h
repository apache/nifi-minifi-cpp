/**
 * @file AWSInitializer.h
 * Initializing AWS SDK
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

#include "aws/core/Aws.h"

// This macro from the Windows headers is used to map the GetMessage
// name to either GetMessageW or GetMessageA depending on the UNICODE
// define. We have to undefine this because it causes a method in the
// AWS sdk to be renamed, causing a compilation error.
// https://github.com/aws/aws-sdk-cpp/issues/402
#if defined(WIN32) && defined(GetMessage)
#undef GetMessage
#endif

#if defined(WIN32) && defined(GetObject)
#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace utils {

class AWSInitializer {
 public:
  static AWSInitializer& get();
  ~AWSInitializer();

 private:
  AWSInitializer();

  Aws::SDKOptions options_;
};

} /* namespace utils */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

/**
 * @file ProcessSessionReadCallback.h
 * ProcessSessionReadCallback class declaration
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
 #ifndef __PROCESS_SESSION_READ_CALLBACK_H__
#define __PROCESS_SESSION_READ_CALLBACK_H__

#include "core/logging/LoggerConfiguration.h"
#include "io/BaseStream.h"
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
  class ProcessSessionReadCallback : public InputStreamCallback {
   public:
       ProcessSessionReadCallback(const std::string &tmpFile, const std::string &destFile,
                 std::shared_ptr<logging::Logger> logger);
       ~ProcessSessionReadCallback();
    virtual int64_t process(std::shared_ptr<io::BaseStream> stream);
    bool commit();

   private:
    std::shared_ptr<logging::Logger> logger_;
    std::ofstream _tmpFileOs;
    bool _writeSucceeded = false;
    std::string _tmpFile;
    std::string _destFile;
  };
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif

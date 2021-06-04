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

#undef NDEBUG

#include <cassert>
#include "../../../libminifi/test/integration/IntegrationBase.h"
#include "core/logging/Logger.h"
#include "../../../libminifi/test/TestBase.h"
#include "../PublishKafka.h"
#include "utils/StringUtils.h"
#include "utils/IntegrationTestUtils.h"

class PublishKafkaOnScheduleTests : public IntegrationBase {
 public:
    void runAssertions() override {
      using org::apache::nifi::minifi::utils::verifyEventHappenedInPollTime;
      assert(verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
        const std::string logs = LogTestController::getInstance().log_output.str();
        const auto result = utils::StringUtils::countOccurrences(logs, "value 1 is outside allowed range 1000..1000000000");
        const int occurrences = result.second;
        return 1 < occurrences;
      }));
      flowController_->updatePropertyValue("kafka", minifi::processors::PublishKafka::MaxMessageSize.getName(), "1999");
      const std::vector<std::string> must_appear_byorder_msgs = {"notifyStop called",
          "Successfully configured PublishKafka",
          "PublishKafka onTrigger"};

      const bool test_success = verifyEventHappenedInPollTime(std::chrono::milliseconds(wait_time_), [&] {
        const std::string logs = LogTestController::getInstance().log_output.str();
        const auto result = utils::StringUtils::countOccurrences(logs, "value 1 is outside allowed range 1000..1000000000");
        size_t last_pos = result.first;
        for (const std::string& msg : must_appear_byorder_msgs) {
          last_pos = logs.find(msg, last_pos);
          if (last_pos == std::string::npos)  {
            return false;
          }
        }
        return true;
      });
      assert(test_success);
    }

    void testSetup() override {
      LogTestController::getInstance().setDebug<core::ProcessGroup>();
      LogTestController::getInstance().setDebug<core::Processor>();
      LogTestController::getInstance().setDebug<core::ProcessSession>();
      LogTestController::getInstance().setDebug<minifi::processors::PublishKafka>();
    }
};

int main(int argc, char **argv) {
  std::string test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
  }

  PublishKafkaOnScheduleTests harness;

  harness.run(test_file_location);

  return 0;
}

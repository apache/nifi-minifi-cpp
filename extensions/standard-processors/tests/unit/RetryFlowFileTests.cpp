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
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <set>
#include <regex>

#include "TestBase.h"
#include "Catch.h"

#include "processors/GenerateFlowFile.h"
#include "processors/UpdateAttribute.h"
#include "processors/RetryFlowFile.h"
#include "processors/PutFile.h"
#include "processors/LogAttribute.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"

namespace {
using std::optional;
namespace FileUtils = org::apache::nifi::minifi::utils::file;

class RetryFlowFileTest {
 public:
  using Processor = org::apache::nifi::minifi::core::Processor;
  using GenerateFlowFile = org::apache::nifi::minifi::processors::GenerateFlowFile;
  using UpdateAttribute = org::apache::nifi::minifi::processors::UpdateAttribute;
  using RetryFlowFile = org::apache::nifi::minifi::processors::RetryFlowFile;
  using PutFile = org::apache::nifi::minifi::processors::PutFile;
  using LogAttribute = org::apache::nifi::minifi::processors::LogAttribute;
  RetryFlowFileTest() :
    logTestController_(LogTestController::getInstance()),
    logger_(logging::LoggerFactory<org::apache::nifi::minifi::processors::RetryFlowFile>::getLogger()) {
    reInitialize();
  }
  virtual ~RetryFlowFileTest() {
    logTestController_.reset();
  }

 protected:
  void reInitialize() {
    testController_ = std::make_unique<TestController>();
    plan_ = testController_->createPlan();
    logTestController_.setDebug<TestPlan>();
    logTestController_.setDebug<GenerateFlowFile>();
    logTestController_.setDebug<UpdateAttribute>();
    logTestController_.setDebug<RetryFlowFile>();
    logTestController_.setDebug<PutFile>();
    logTestController_.setDebug<PutFile>();
    logTestController_.setDebug<LogAttribute>();
    logTestController_.setDebug<core::ProcessSession>();
    logTestController_.setDebug<core::Connectable>();
    logTestController_.setDebug<minifi::Connection>();
  }

  void retryRoutingTest(
      const optional<std::string>& /*exp_retry_prop_name*/,
      optional<int> /*exp_retry_prop_val*/,
      const core::Relationship& exp_outbound_relationship,
      bool exp_penalty_on_flowfile,
      const optional<std::string>& retry_attr_name_on_flowfile,
      const optional<std::string>& retry_attribute_value_before_processing,
      optional<int> maximum_retries,
      optional<bool> penalize_retries,
      optional<bool> fail_on_non_numerical_overwrite,
      const optional<std::string>& reuse_mode,
      optional<bool> processor_uuid_matches_flowfile) {
    reInitialize();

    // Relationships
    const core::Relationship success         {"success", "description"};
    const core::Relationship retry           {RetryFlowFile::Retry};
    const core::Relationship retries_exceeded{RetryFlowFile::RetriesExceeded};
    const core::Relationship failure         {RetryFlowFile::Failure};

    // Processors
    std::shared_ptr<core::Processor> generate                    = plan_->addProcessor("GenerateFlowFile", "generate", {success}, false);
    std::shared_ptr<core::Processor> update                      = plan_->addProcessor("UpdateAttribute", "update", {success}, false);
    std::shared_ptr<core::Processor> retryflowfile               = plan_->addProcessor("RetryFlowFile", "retryflowfile", {retry, retries_exceeded, failure}, false);
    std::shared_ptr<core::Processor> putfile_on_retry            = plan_->addProcessor("PutFile", "putfile_on_retry", {success}, false);
    std::shared_ptr<core::Processor> putfile_on_retries_exceeded = plan_->addProcessor("PutFile", "putfile_on_retries_exceeded", {success}, false);
    std::shared_ptr<core::Processor> putfile_on_failure          = plan_->addProcessor("PutFile", "putfile_on_failure", {success}, false);
    std::shared_ptr<core::Processor> log_attribute               = plan_->addProcessor("LogAttribute", "log", {success}, false);

    retryflowfile->setPenalizationPeriod(std::chrono::milliseconds{0});

    plan_->addConnection(generate, success, update);
    plan_->addConnection(update, success, retryflowfile);
    plan_->addConnection(retryflowfile, retry, putfile_on_retry);
    plan_->addConnection(retryflowfile, retries_exceeded, putfile_on_retries_exceeded);
    plan_->addConnection(retryflowfile, failure, putfile_on_failure);
    plan_->addConnection(putfile_on_retry, success, log_attribute);
    plan_->addConnection(putfile_on_retries_exceeded, success, log_attribute);
    plan_->addConnection(putfile_on_failure, success, log_attribute);


    update->setAutoTerminatedRelationships(std::array{failure});
    putfile_on_retry->setAutoTerminatedRelationships(std::array{failure});
    putfile_on_retries_exceeded->setAutoTerminatedRelationships(std::array{failure});
    putfile_on_failure->setAutoTerminatedRelationships(std::array{failure});
    log_attribute->setAutoTerminatedRelationships(std::array{success});

    // Properties
    if (retry_attribute_value_before_processing) { plan_->setDynamicProperty(update, retry_attr_name_on_flowfile.value_or("flowfile.retries"), retry_attribute_value_before_processing.value()); }

    if (processor_uuid_matches_flowfile) {
      if (processor_uuid_matches_flowfile.value()) {
        plan_->setDynamicProperty(update, retry_attr_name_on_flowfile.value_or("flowfile.retries") + ".uuid", retryflowfile->getUUIDStr().view());
      } else {
        utils::Identifier non_matching_uuid = utils::IdGenerator::getIdGenerator()->generate();
        plan_->setDynamicProperty(update, retry_attr_name_on_flowfile.value_or("flowfile.retries") + ".uuid", non_matching_uuid.to_string().view());
      }
    }

    if (maximum_retries)                 { plan_->setProperty(retryflowfile, RetryFlowFile::MaximumRetries, std::to_string(maximum_retries.value())); }
    if (penalize_retries)                { plan_->setProperty(retryflowfile, RetryFlowFile::PenalizeRetries, penalize_retries.value() ? "true": "false"); }
    if (fail_on_non_numerical_overwrite) { plan_->setProperty(retryflowfile, RetryFlowFile::FailOnNonNumericalOverwrite, fail_on_non_numerical_overwrite.value() ? "true": "false"); }
    if (reuse_mode)                      { plan_->setProperty(retryflowfile, RetryFlowFile::ReuseMode, reuse_mode.value()); }
    plan_->setDynamicProperty(retryflowfile, "retries_exceeded_property_key_1", "retries_exceeded_property_value_1");
    plan_->setDynamicProperty(retryflowfile, "retries_exceeded_property_key_2", "retries_exceeded_property_value_2");

    const auto retry_dir            = testController_->createTempDirectory();
    const auto retries_exceeded_dir = testController_->createTempDirectory();
    const auto failure_dir          = testController_->createTempDirectory();

    plan_->setProperty(putfile_on_retry, PutFile::Directory, retry_dir.string());
    plan_->setProperty(putfile_on_retries_exceeded, PutFile::Directory, retries_exceeded_dir.string());
    plan_->setProperty(putfile_on_failure, PutFile::Directory, failure_dir.string());

    plan_->runNextProcessor();  // GenerateFlowFile
    plan_->runNextProcessor();  // UpdateAttribute
    plan_->runNextProcessor();  // RetryFlowFile
    plan_->runNextProcessor();  // PutFile
    plan_->runNextProcessor();  // PutFile
    plan_->runNextProcessor();  // PutFile
    plan_->runNextProcessor();  // LogAttribute

    REQUIRE((RetryFlowFile::Retry.name == exp_outbound_relationship.getName() ? 1 : 0) == FileUtils::list_dir_all(retry_dir, logger_).size());
    REQUIRE((RetryFlowFile::RetriesExceeded.name == exp_outbound_relationship.getName() ? 1 : 0) == FileUtils::list_dir_all(retries_exceeded_dir, logger_).size());
    REQUIRE((RetryFlowFile::Failure.name == exp_outbound_relationship.getName() ? 1 : 0) == FileUtils::list_dir_all(failure_dir, logger_).size());
    REQUIRE((RetryFlowFile::RetriesExceeded.name == exp_outbound_relationship.getName()) == logContainsText("key:retries_exceeded_property_key_1 value:retries_exceeded_property_value_1"));
    REQUIRE((RetryFlowFile::RetriesExceeded.name == exp_outbound_relationship.getName()) == logContainsText("key:retries_exceeded_property_key_2 value:retries_exceeded_property_value_2"));
    REQUIRE(exp_penalty_on_flowfile == flowfileWasPenalizedARetryflowfile());
    const bool expect_warning_on_reuse = !processor_uuid_matches_flowfile.value_or(true) && "Warn on Reuse" == reuse_mode;
    REQUIRE(expect_warning_on_reuse == retryFlowfileWarnedForReuse());
  }

  static bool logContainsText(const std::string& pattern) {
    const std::string logs = LogTestController::getInstance().getLogs();
    return logs.find(pattern) != std::string::npos;
  }

  static bool flowfileWasPenalizedARetryflowfile() {
    std::regex re(R"(\[org::apache::nifi::minifi::core::ProcessSession\] \[info\] Penalizing [0-9a-z\-]+ for [0-9]*ms at retryflowfile)");
    return std::regex_search(LogTestController::getInstance().getLogs(), re);
  }

  static bool retryFlowfileWarnedForReuse() {
    const std::string pattern = "[org::apache::nifi::minifi::processors::RetryFlowFile] [warning] Reusing retry attribute that belongs to different processor. Resetting value to 0.";
    return logContainsText(pattern);
  }

  std::unique_ptr<TestController> testController_;
  std::shared_ptr<TestPlan> plan_;
  LogTestController& logTestController_;
  std::shared_ptr<logging::Logger> logger_;
};

TEST_CASE_METHOD(RetryFlowFileTest, "Simple file passthrough", "[executePythonProcessorSimple]") {
  // RetryFlowFile outbound relationships
  const core::Relationship retry           {RetryFlowFile::Retry};
  const core::Relationship retries_exceeded{RetryFlowFile::RetriesExceeded};
  const core::Relationship failure         {RetryFlowFile::Failure};

  //                 EXP_RETRY_PROP_NAME                           RETRY_ATTRIBUTE_VALUE_BEFORE_PROCESSING FAIL_NONNUM_OVERW
  //                       EXP_RETRY_PROP_VAL        EXP_PENALTY_ON_FF                        MAXIMUM_RETRIES                     REUSE_MODE
  //                                  EXP_OUTBOUND_RELATIONSHIP   RETRY_ATTR_NAME_ON_FLOWFILE        PENALIZE_RETRIES     PROC_UUID_MATCHES_FLOWFILE
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,                    {},          {}, {},    {},    {},              {},    {}); // NOLINT
  retryRoutingTest("flowfile.retryCount",  1,            retry,  true, "flowfile.retryCount",          {}, {},    {},    {},              {},    {}); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1", {},    {},    {},              {},  true); // NOLINT
  retryRoutingTest(                   {}, {}, retries_exceeded, false,    "flowfile.retries",         "3", {},    {},    {},              {},  true); // NOLINT
  retryRoutingTest(                   {}, {}, retries_exceeded, false,    "flowfile.retries",         "4", {},    {},    {},              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  6,            retry,  true,    "flowfile.retries",         "5",  6,    {},    {},              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,    "flowfile.retries",         "2", {},  true,    {},              {},  true); // NOLINT
  retryRoutingTest(                   {}, {}, retries_exceeded, false,    "flowfile.retries",         "3", {},  true,    {},              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry, false,    "flowfile.retries",         "2", {}, false,    {},              {},  true); // NOLINT
  retryRoutingTest(                   {}, {}, retries_exceeded, false,    "flowfile.retries",         "3", {}, false,    {},              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1", {},    {},  true,              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1",  6,    {}, false,              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,    "flowfile.retries", "incorrect", {},    {},    {},              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries", {},          failure, false,    "flowfile.retries", "incorrect", {},    {},  true,              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,    "flowfile.retries", "incorrect",  6,    {}, false,              {},  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1", {},    {},    {}, "Fail on Reuse",  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1", {},    {},    {}, "Warn on Reuse",  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  2,            retry,  true,    "flowfile.retries",         "1", {},    {},    {},   "Reset Reuse",  true); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,          failure, false,    "flowfile.retries",         "1", {},    {},    {},              {}, false); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,          failure, false,    "flowfile.retries",         "1", {},    {},    {}, "Fail on Reuse", false); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,    "flowfile.retries",         "1", {},    {},    {}, "Warn on Reuse", false); // NOLINT
  retryRoutingTest(   "flowfile.retries",  1,            retry,  true,    "flowfile.retries",         "1", {},    {},    {},   "Reset Reuse", false); // NOLINT
}
}  // namespace

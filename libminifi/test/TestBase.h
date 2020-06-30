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

#ifndef LIBMINIFI_TEST_TESTBASE_H_
#define LIBMINIFI_TEST_TESTBASE_H_
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "ResourceClaim.h"
#include "utils/file/FileUtils.h"
#include "catch.hpp"
#include <vector>
#include <set>
#include <map>
#include "core/logging/Logger.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "properties/Properties.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "spdlog/common.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessContextBuilder.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "core/state/nodes/FlowInformation.h"
#include "properties/Configure.h"
#include "utils/ClassUtils.h"

class LogTestController {
 public:
  ~LogTestController() = default;
  static LogTestController& getInstance() {
    static LogTestController instance;
    return instance;
  }

  static std::shared_ptr<LogTestController> getInstance(const std::shared_ptr<logging::LoggerProperties> &logger_properties) {
    static std::map<std::shared_ptr<logging::LoggerProperties>, std::shared_ptr<LogTestController>> map;
    auto fnd = map.find(logger_properties);
    if (fnd != std::end(map)) {
      return fnd->second;
    } else {
      // in practice I'd use a derivation here or another paradigm entirely but for the purposes of this test code
      // having extra overhead is negligible. this is the most readable and least impactful way
      auto instance = std::shared_ptr<LogTestController>(new LogTestController(logger_properties));
      map.insert(std::make_pair(logger_properties, instance));
      return map.find(logger_properties)->second;
    }
  }

  template<typename T>
  void setTrace() {
    setLevel<T>(spdlog::level::trace);
  }

  template<typename T>
  void setDebug() {
    setLevel<T>(spdlog::level::debug);
  }

  template<typename T>
  void setInfo() {
    setLevel<T>(spdlog::level::info);
  }

  template<typename T>
  void setWarn() {
    setLevel<T>(spdlog::level::warn);
  }

  template<typename T>
  void setError() {
    setLevel<T>(spdlog::level::err);
  }

  template<typename T>
  void setOff() {
    setLevel<T>(spdlog::level::off);
  }

  /**
   * Most tests use the main logging framework. this addition allows us to have and control variants for the purposes
   * of changeable test formats
   */
  template<typename T>
  std::shared_ptr<logging::Logger> getLogger() {
    std::string name = core::getClassName<T>();
    return config ? config->getLogger(name) : logging::LoggerConfiguration::getConfiguration().getLogger(name);
  }

  template<typename T>
  void setLevel(spdlog::level::level_enum level) {
    logging::LoggerFactory<T>::getLogger();
    std::string name = core::getClassName<T>();
    if (config)
      config->getLogger(name);
    else
      logging::LoggerConfiguration::getConfiguration().getLogger(name);
    modified_loggers.insert(name);
    setLevel(name, level);
    // also support shortened classnames
    if (config && config->shortenClassNames()) {
      std::string adjusted = name;
      if (utils::ClassUtils::shortenClassName(name, adjusted)) {
        modified_loggers.insert(name);
        setLevel(name, level);
      }
    }

  }

  bool contains(const std::string &ending, std::chrono::seconds timeout = std::chrono::seconds(3)) {
    return contains(log_output, ending, timeout);
  }

  bool contains(const std::ostringstream &stream, const std::string &ending, std::chrono::seconds timeout = std::chrono::seconds(3)) {
    if (ending.length() == 0) {
      return false;
    }
    auto start = std::chrono::system_clock::now();
    bool found = false;
    bool timed_out = false;
    do {
      std::string str = stream.str();
      found = (str.find(ending) != std::string::npos);
      auto now = std::chrono::system_clock::now();
      timed_out = std::chrono::duration_cast<std::chrono::milliseconds>(now - start) > std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
      if (!found && !timed_out) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    } while (!found && !timed_out);

    logger_->log_info("%s %s in log output.", found ? "Successfully found" : "Failed to find", ending);
    return found;
  }

  void reset() {
    for (auto const & name : modified_loggers) {
      setLevel(name, spdlog::level::err);
    }
    modified_loggers.clear();
    if (config)
      config = logging::LoggerConfiguration::newInstance();
    resetStream(log_output);
  }

  inline void resetStream(std::ostringstream &stream) {
    stream.str("");
    stream.clear();
  }

  std::ostringstream log_output;

  std::shared_ptr<logging::Logger> logger_;
 protected:
  class TestBootstrapLogger : public logging::Logger {
   public:
    TestBootstrapLogger(std::shared_ptr<spdlog::logger> logger)
        : Logger(logger) {
    }
  };
  LogTestController()
      : LogTestController(nullptr) {
  }

  explicit LogTestController(const std::shared_ptr<logging::LoggerProperties> &loggerProps) {
    my_properties_ = loggerProps;
    bool initMain = false;
    if (nullptr == my_properties_) {
      my_properties_ = std::make_shared<logging::LoggerProperties>();
      initMain = true;
    }
    my_properties_->set("logger.root", "ERROR,ostream");
    my_properties_->set("logger." + core::getClassName<LogTestController>(), "INFO");
    my_properties_->set("logger." + core::getClassName<logging::LoggerConfiguration>(), "DEBUG");
    std::shared_ptr<spdlog::sinks::dist_sink_mt> dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    dist_sink->add_sink(std::make_shared<spdlog::sinks::ostream_sink_mt>(log_output, true));
    dist_sink->add_sink(spdlog::sinks::stderr_sink_mt::instance());
    my_properties_->add_sink("ostream", dist_sink);
    if (initMain) {
      logging::LoggerConfiguration::getConfiguration().initialize(my_properties_);
      logger_ = logging::LoggerConfiguration::getConfiguration().getLogger(core::getClassName<LogTestController>());
    } else {
      config = logging::LoggerConfiguration::newInstance();
      // create for test purposes. most tests use the main logging factory, but this exists to test the logging
      // framework itself.
      config->initialize(my_properties_);
      logger_ = config->getLogger(core::getClassName<LogTestController>());
    }

  }
  LogTestController(LogTestController const&);
  LogTestController& operator=(LogTestController const&);

  void setLevel(const std::string name, spdlog::level::level_enum level);

  std::shared_ptr<logging::LoggerProperties> my_properties_;
  std::unique_ptr<logging::LoggerConfiguration> config;
  std::set<std::string> modified_loggers;
};

class TestPlan {
 public:

  explicit TestPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo,
                    const std::shared_ptr<minifi::state::response::FlowVersion> &flow_version, const std::shared_ptr<minifi::Configure> &configuration, const char* state_dir);

  virtual ~TestPlan();

  std::shared_ptr<core::Processor> addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name,
                                                core::Relationship relationship = core::Relationship("success", "description"), bool linkToPrevious = false) {
    return addProcessor(processor, name, { relationship }, linkToPrevious);
  }

  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const std::string &name, core::Relationship relationship = core::Relationship("success", "description"),
                                                bool linkToPrevious = false) {
    return addProcessor(processor_name, name, { relationship }, linkToPrevious);
  }

  std::shared_ptr<core::Processor> addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name, const std::initializer_list<core::Relationship>& relationships,
                                                bool linkToPrevious = false);

  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const std::string &name, const std::initializer_list<core::Relationship>& relationships,
                                                bool linkToPrevious = false);

  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, utils::Identifier& uuid, const std::string &name, const std::initializer_list<core::Relationship>& relationships,
                                                bool linkToPrevious = false);

  std::shared_ptr<core::controller::ControllerServiceNode> addController(const std::string &controller_name, const std::string &name);

  bool setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value, bool dynamic = false);

  bool setProperty(const std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node, const std::string &prop, const std::string &value, bool dynamic = false);

  void reset(bool reschedule = false);

  bool runNextProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify = nullptr);

  bool runCurrentProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify = nullptr);

  std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> getProvenanceRecords();

  std::shared_ptr<core::FlowFile> getCurrentFlowFile();

  std::shared_ptr<core::ProcessContext> getCurrentContext();

  std::shared_ptr<core::Repository> getFlowRepo() {
    return flow_repo_;
  }

  std::shared_ptr<core::Repository> getProvenanceRepo() {
    return prov_repo_;
  }

  std::shared_ptr<core::ContentRepository> getContentRepo() {
    return content_repo_;
  }

  std::shared_ptr<logging::Logger> getLogger() const {
    return logger_;
  }

  std::string getStateDir() {
    return state_dir_;
  }

  std::shared_ptr<core::CoreComponentStateManagerProvider> getStateManagerProvider() {
    return state_manager_provider_;
  }

  void finalize();

 protected:

  std::shared_ptr<minifi::Connection> buildFinalConnection(std::shared_ptr<core::Processor> processor, bool setDest = false);

  std::shared_ptr<org::apache::nifi::minifi::io::StreamFactory> stream_factory;

  std::shared_ptr<minifi::Configure> configuration_;

  std::shared_ptr<core::ContentRepository> content_repo_;

  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::Repository> prov_repo_;

  std::shared_ptr<core::controller::ControllerServiceMap> controller_services_;
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider_;

  std::shared_ptr<core::CoreComponentStateManagerProvider> state_manager_provider_;

  std::string state_dir_;

  std::recursive_mutex mutex;

  std::atomic<bool> finalized;

  int location;

  std::shared_ptr<core::FlowFile> current_flowfile_;

  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> controller_service_nodes_;
  std::map<std::string, std::shared_ptr<core::Processor>> processor_mapping_;
  std::vector<std::shared_ptr<core::Processor>> processor_queue_;
  std::vector<std::shared_ptr<core::Processor>> configured_processors_;
  std::vector<std::shared_ptr<core::ProcessorNode>> processor_nodes_;
  std::vector<std::shared_ptr<core::ProcessContext>> processor_contexts_;
  std::vector<std::shared_ptr<core::ProcessSession>> process_sessions_;
  std::vector<std::shared_ptr<core::ProcessSessionFactory>> factories_;
  std::vector<std::shared_ptr<minifi::Connection>> relationships_;
  core::Relationship termination_;

 private:

  std::shared_ptr<logging::Logger> logger_;
};

class TestController {
 public:

  TestController()
      : log(LogTestController::getInstance()) {
    core::FlowConfiguration::initialize_static_functions();
    minifi::setDefaultDirectory("./");
    log.reset();
    utils::IdGenerator::getIdGenerator()->initialize(std::make_shared<minifi::Properties>());
    flow_version_ = std::make_shared<minifi::state::response::FlowVersion>("test", "test", "test");
  }

  std::shared_ptr<TestPlan> createPlan(std::shared_ptr<minifi::Configure> configuration = nullptr, const char* state_dir = nullptr) {
    if (configuration == nullptr) {
      configuration = std::make_shared<minifi::Configure>();
    }
    std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

    content_repo->initialize(configuration);

    std::shared_ptr<core::Repository> flow_repo = std::make_shared<TestRepository>();
    std::shared_ptr<core::Repository> repo = std::make_shared<TestRepository>();
    return std::make_shared<TestPlan>(content_repo, flow_repo, repo, flow_version_, configuration, state_dir);
  }

  void runSession(std::shared_ptr<TestPlan> &plan, bool runToCompletion = true, std::function<void(const std::shared_ptr<core::ProcessContext>&, const std::shared_ptr<core::ProcessSession>&)> verify =
                      nullptr) {

    while (plan->runNextProcessor(verify) && runToCompletion) {

    }
  }

  const std::shared_ptr<logging::Logger>& getLogger() const {
    return log.logger_;
  }

  ~TestController() {
    for (auto dir : directories) {
      utils::file::FileUtils::delete_dir(dir, true);
    }
  }

  /**
   * format will be changed by mkdtemp, so don't rely on a shared variable.
   */
  std::string createTempDirectory(char *format) {
    const auto dir = utils::file::FileUtils::create_temp_directory(format);
    directories.push_back(dir);
    return dir;
  }

 protected:

  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;

  std::mutex test_mutex;

  LogTestController &log;
  std::vector<std::string> directories;

};

#endif /* LIBMINIFI_TEST_TESTBASE_H_ */

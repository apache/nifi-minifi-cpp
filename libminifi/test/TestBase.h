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
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>
#include "ResourceClaim.h"
#include "utils/file/FileUtils.h"
#include "catch.hpp"
#include "core/logging/Logger.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "properties/Properties.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

#include "spdlog/common.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessContextBuilder.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "core/state/nodes/FlowInformation.h"
#include "utils/ClassUtils.h"
#include "Path.h"

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

  bool contains(const std::string &ending, std::chrono::seconds timeout = std::chrono::seconds(3), std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200)) {
    return contains(log_output, ending, timeout, sleep_interval);
  }

  bool contains(const std::ostringstream &stream, const std::string &ending, std::chrono::seconds timeout = std::chrono::seconds(3), std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200)) {
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
        std::this_thread::sleep_for(sleep_interval);
      }
    } while (!found && !timed_out);

    logger_->log_info("%s %s in log output.", found ? "Successfully found" : "Failed to find", ending);
    return found;
  }

  int countOccurrences(const std::string& pattern) {
    return utils::StringUtils::countOccurrences(log_output.str(), pattern).second;
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
    my_properties_->set("logger." + core::getClassName<logging::LoggerConfiguration>(), "INFO");
    std::shared_ptr<spdlog::sinks::dist_sink_mt> dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    dist_sink->add_sink(std::make_shared<spdlog::sinks::ostream_sink_mt>(log_output, true));
    dist_sink->add_sink(std::make_shared<spdlog::sinks::stderr_sink_mt>());
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
  std::shared_ptr<core::Processor> addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name, const std::initializer_list<core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT
  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const std::string &name, const std::initializer_list<core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT
  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const utils::Identifier& uuid, const std::string &name, const std::initializer_list<core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT

  std::shared_ptr<minifi::Connection> addConnection(const std::shared_ptr<core::Processor>& source_proc, const core::Relationship& source_relationship, const std::shared_ptr<core::Processor>& destination_proc); // NOLINT

  std::shared_ptr<core::controller::ControllerServiceNode> addController(const std::string &controller_name, const std::string &name);

  bool setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value, bool dynamic = false);

  bool setProperty(const std::shared_ptr<core::controller::ControllerServiceNode> controller_service_node, const std::string &prop, const std::string &value, bool dynamic = false);

  void reset(bool reschedule = false);
  void increment_location() { ++location; }
  void reset_location() { location = -1; }

  using PreTriggerVerifier = std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)>;

  std::vector<std::shared_ptr<core::Processor>>::iterator getProcessorItByUuid(const std::string& uuid);
  std::shared_ptr<core::ProcessContext> getProcessContextForProcessor(const std::shared_ptr<core::Processor>& processor);

  void scheduleProcessor(const std::shared_ptr<core::Processor>& processor, const std::shared_ptr<core::ProcessContext>& context);
  void scheduleProcessor(const std::shared_ptr<core::Processor>& processor);
  void scheduleProcessors();

  // Note: all this verify logic is only used in TensorFlow tests as a replacement for UpdateAttribute
  // It should probably not be the part of the standard way of running processors
  bool runProcessor(const std::shared_ptr<core::Processor>& processor, const PreTriggerVerifier& verify = nullptr);
  bool runProcessor(size_t target_location, const PreTriggerVerifier& verify = nullptr);
  bool runNextProcessor(const PreTriggerVerifier& verify = nullptr);
  bool runCurrentProcessor(const PreTriggerVerifier& verify = nullptr);
  bool runCurrentProcessorUntilFlowfileIsProduced(const std::chrono::seconds& wait_duration);

  std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> getProvenanceRecords();

  std::shared_ptr<core::FlowFile> getCurrentFlowFile();
  std::vector<minifi::Connection*> getProcessorOutboundConnections(const std::shared_ptr<core::Processor>& processor);
  std::size_t getNumFlowFileProducedByCurrentProcessor();
  std::shared_ptr<core::FlowFile> getFlowFileProducedByCurrentProcessor();

  std::shared_ptr<core::ProcessContext> getCurrentContext();

  std::shared_ptr<core::Repository> getFlowRepo() {
    return flow_repo_;
  }

  std::shared_ptr<core::ContentRepository> getContentRepo() {
    return content_repo_;
  }

  std::shared_ptr<core::Repository> getProvenanceRepo() {
    return prov_repo_;
  }

  std::shared_ptr<logging::Logger> getLogger() const {
    return logger_;
  }

  std::string getStateDir() const {
    return state_dir_->getPath();
  }

  std::shared_ptr<core::CoreComponentStateManagerProvider> getStateManagerProvider() {
    return state_manager_provider_;
  }

  std::string getContent(const std::shared_ptr<core::FlowFile>& file) {
    auto content_claim = file->getResourceClaim();
    auto content_stream = content_repo_->read(*content_claim.get());
    auto output_stream = std::make_shared<minifi::io::BufferStream>();
    minifi::InputStreamPipe{output_stream}.process(content_stream);
    return {reinterpret_cast<const char*>(output_stream->getBuffer()), output_stream->size()};
  }

  void finalize();

 protected:
  class StateDir {
   public:
    StateDir() {
      char state_dir_name_template[] = "/var/tmp/teststate.XXXXXX";
      path_ = utils::file::FileUtils::create_temp_directory(state_dir_name_template);
      is_owner_ = true;
    }

    StateDir(std::string path) : path_(std::move(path)), is_owner_(false) {}

    StateDir(const StateDir&) = delete;
    StateDir& operator=(const StateDir&) = delete;

    ~StateDir() {
      if (is_owner_) {
        utils::file::FileUtils::delete_dir(path_, true);
      }
    }

    std::string getPath() const {
      return path_;
    }

   private:
    std::string path_;
    bool is_owner_;
  };

  std::unique_ptr<StateDir> state_dir_;

  std::shared_ptr<minifi::Connection> buildFinalConnection(std::shared_ptr<core::Processor> processor, bool setDest = false);

  std::shared_ptr<org::apache::nifi::minifi::io::StreamFactory> stream_factory;

  std::shared_ptr<minifi::Configure> configuration_;

  std::shared_ptr<core::ContentRepository> content_repo_;

  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::Repository> prov_repo_;

  std::shared_ptr<core::controller::ControllerServiceMap> controller_services_;
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider_;

  std::shared_ptr<core::CoreComponentStateManagerProvider> state_manager_provider_;

  std::recursive_mutex mutex;

  std::atomic<bool> finalized;

  int location;

  std::shared_ptr<core::FlowFile> current_flowfile_;

  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> controller_service_nodes_;
  std::map<utils::Identifier, std::shared_ptr<core::Processor>> processor_mapping_;
  std::vector<std::shared_ptr<core::Processor>> processor_queue_;
  std::vector<std::shared_ptr<core::Processor>> configured_processors_;  // Do not assume ordering
  std::vector<std::shared_ptr<core::ProcessorNode>> processor_nodes_;
  std::vector<std::shared_ptr<core::ProcessContext>> processor_contexts_;
  std::vector<std::shared_ptr<core::ProcessSession>> process_sessions_;
  std::vector<std::shared_ptr<core::ProcessSessionFactory>> factories_;  // Do not assume ordering
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
      configuration->set(minifi::Configure::nifi_state_management_provider_local_class_name, "UnorderedMapKeyValueStoreService");
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

  const LogTestController& getLog() const {
    return log;
  }

  ~TestController() {
    for (const auto& dir : directories) {
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
  LogTestController &log;
  std::vector<std::string> directories;
};

#endif /* LIBMINIFI_TEST_TESTBASE_H_ */

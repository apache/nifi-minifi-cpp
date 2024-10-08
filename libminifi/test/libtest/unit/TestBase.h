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
#undef NDEBUG
#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <mutex>

#include "spdlog/common.h"

#include "core/Core.h"
#include "core/extension/ExtensionManager.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/LoggerProperties.h"
#include "core/Relationship.h"
#include "core/repository/VolatileContentRepository.h"
#include "utils/file/FileUtils.h"
#include "properties/Configuration.h"

namespace minifi = org::apache::nifi::minifi;
namespace utils = minifi::utils;
namespace logging = minifi::core::logging;
namespace core = minifi::core;

namespace org::apache::nifi::minifi {
class Connection;
namespace core {
class StateStorage;
class ContentRepository;
class FlowFile;
class Processor;
class ProcessorNode;
class ProcessContext;
class ProcessSession;
class ProcessSessionFactory;
class Repository;
namespace controller {
class ControllerServiceNode;
class ControllerServiceNodeMap;
class ControllerServiceProvider;
}  // namespace controller
}  // namespace core
namespace state::response {
class FlowVersion;
}  // namespace state::response
namespace provenance {
class ProvenanceEventRecord;
}  // namespace provenance
}  // namespace org::apache::nifi::minifi

class LogTestController {
 public:
  ~LogTestController() = default;
  static LogTestController& getInstance() {
    static LogTestController instance;
    return instance;
  }

  static std::shared_ptr<LogTestController> getInstance(const std::shared_ptr<logging::LoggerProperties> &logger_properties);

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
  std::shared_ptr<logging::Logger> getLogger(const std::optional<utils::Identifier>& id = {}) { return getLoggerByClassName(minifi::core::className<T>(), id); }

  std::shared_ptr<logging::Logger> getLoggerByClassName(std::string_view class_name, const std::optional<utils::Identifier>& id = {});

  template<typename T>
  void setLevel(spdlog::level::level_enum level) {
    setLevelByClassName(level, minifi::core::className<T>());
  }

  void setLevelByClassName(spdlog::level::level_enum level, std::string_view class_name);

  bool contains(const std::string &ending, std::chrono::milliseconds timeout = std::chrono::seconds(3), std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200)) const;

  static bool contains(const std::ostringstream &stream, const std::string &ending,
      std::chrono::milliseconds timeout = std::chrono::seconds(3),
      std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200));

  std::optional<std::smatch> matchesRegex(const std::string &regex_str,
                std::chrono::milliseconds timeout = std::chrono::seconds(3),
                std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200)) const;

  int countOccurrences(const std::string& pattern) const;

  void reset();

  void clear();

  static void resetStream(std::ostringstream &stream);

  std::string getLogs() const {
    std::lock_guard<std::mutex> guard(*log_output_mutex_);
    gsl_Expects(log_output_ptr_);
    return log_output_ptr_->str();
  }

  std::shared_ptr<logging::Logger> logger_;

 protected:
  LogTestController()
      : LogTestController(nullptr) {
  }

  explicit LogTestController(const std::shared_ptr<logging::LoggerProperties> &loggerProps);

  void init(const std::shared_ptr<logging::LoggerProperties>& logger_props);
  void setLevel(std::string_view name, spdlog::level::level_enum level);
  static bool contains(const std::function<std::string()>& log_string_getter, const std::string& ending, std::chrono::milliseconds timeout, std::chrono::milliseconds sleep_interval);

  mutable std::shared_ptr<std::mutex> log_output_mutex_ = std::make_shared<std::mutex>();
  std::shared_ptr<std::ostringstream> log_output_ptr_ = std::make_shared<std::ostringstream>();
  std::shared_ptr<logging::LoggerProperties> my_properties_;
  std::unique_ptr<logging::LoggerConfiguration> config;
  std::vector<std::string> modified_loggers;
};

class TempDirectory {
 public:
  TempDirectory() {
    char format[] = "/var/tmp/nifi-minifi-cpp.test.XXXXXX";
    path_ = minifi::utils::file::FileUtils::create_temp_directory(format);
    is_owner_ = true;
  }
  explicit TempDirectory(std::filesystem::path path): path_{std::move(path)}, is_owner_{false} {}

  // disable copy
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;

  ~TempDirectory() {
    if (is_owner_) {
      assert(minifi::utils::file::FileUtils::delete_dir(path_, true) == 0);
    }
  }

  [[nodiscard]]
  std::filesystem::path getPath() const {
    return path_;
  }

 private:
  std::filesystem::path path_;
  bool is_owner_;
};

class TestPlan {
 public:
  explicit TestPlan(std::shared_ptr<minifi::core::ContentRepository> content_repo, std::shared_ptr<minifi::core::Repository> flow_repo, std::shared_ptr<minifi::core::Repository> prov_repo,
                    std::shared_ptr<minifi::state::response::FlowVersion> flow_version, std::shared_ptr<minifi::Configure> configuration, const char* state_dir);

  virtual ~TestPlan();

  std::shared_ptr<minifi::core::Processor> addProcessor(const std::shared_ptr<minifi::core::Processor> &processor, const std::string &name,
      const minifi::core::Relationship& relationship = minifi::core::Relationship("success", "description"), bool linkToPrevious = false) {
    return addProcessor(processor, name, { relationship }, linkToPrevious);
  }
  std::shared_ptr<minifi::core::Processor> addProcessor(const std::string &processor_name, const std::string &name,
      const minifi::core::Relationship& relationship = minifi::core::Relationship("success", "description"), bool linkToPrevious = false) {
    return addProcessor(processor_name, name, { relationship }, linkToPrevious);
  }
  std::shared_ptr<minifi::core::Processor> addProcessor(const std::shared_ptr<minifi::core::Processor> &processor, const std::string &name, const std::initializer_list<minifi::core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT
  std::shared_ptr<minifi::core::Processor> addProcessor(const std::string &processor_name, const std::string &name, const std::initializer_list<minifi::core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT
  std::shared_ptr<minifi::core::Processor> addProcessor(const std::string &processor_name, const minifi::utils::Identifier& uuid, const std::string &name, const std::initializer_list<minifi::core::Relationship>& relationships, bool linkToPrevious = false); // NOLINT

  minifi::Connection* addConnection(const std::shared_ptr<minifi::core::Processor>& source_proc, const minifi::core::Relationship& source_relationship, const std::shared_ptr<minifi::core::Processor>& destination_proc); // NOLINT

  std::shared_ptr<minifi::core::controller::ControllerServiceNode> addController(const std::string &controller_name, const std::string &name);

  bool setProperty(const std::shared_ptr<minifi::core::Processor>& processor, const core::PropertyReference& property, std::string_view value);
  bool setProperty(const std::shared_ptr<minifi::core::Processor>& processor, std::string_view property, std::string_view value);
  bool setDynamicProperty(const std::shared_ptr<minifi::core::Processor>& processor, std::string_view property, std::string_view value);

  static bool setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, const core::PropertyReference& property, std::string_view value);
  static bool setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, std::string_view property, std::string_view value);
  static bool setDynamicProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, std::string_view property, std::string_view value);

  void reset(bool reschedule = false);
  void increment_location() { ++location; }
  void reset_location() { location = -1; }

  using PreTriggerVerifier = std::function<void(const std::shared_ptr<minifi::core::ProcessContext>, const std::shared_ptr<minifi::core::ProcessSession>)>;

  std::vector<std::shared_ptr<minifi::core::Processor>>::iterator getProcessorItByUuid(const std::string& uuid);
  std::shared_ptr<minifi::core::ProcessContext> getProcessContextForProcessor(const std::shared_ptr<minifi::core::Processor>& processor);

  void scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const std::shared_ptr<minifi::core::ProcessContext>& context);
  void scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor);
  void scheduleProcessors();

  bool runProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const PreTriggerVerifier& verify = nullptr);
  bool runProcessor(size_t target_location, const PreTriggerVerifier& verify = nullptr);
  bool runNextProcessor(const PreTriggerVerifier& verify = nullptr);
  bool runCurrentProcessor();
  bool runCurrentProcessorUntilFlowfileIsProduced(std::chrono::milliseconds wait_duration);

  std::set<std::shared_ptr<minifi::provenance::ProvenanceEventRecord>> getProvenanceRecords();

  std::shared_ptr<minifi::core::FlowFile> getCurrentFlowFile();
  std::vector<minifi::Connection*> getProcessorOutboundConnections(const std::shared_ptr<minifi::core::Processor>& processor);
  std::size_t getNumFlowFileProducedByProcessor(const std::shared_ptr<minifi::core::Processor>& processor);
  std::size_t getNumFlowFileProducedByCurrentProcessor();
  std::shared_ptr<minifi::core::FlowFile> getFlowFileProducedByCurrentProcessor();

  std::shared_ptr<minifi::core::ProcessContext> getCurrentContext();

  std::shared_ptr<minifi::core::Repository> getFlowRepo() {
    return flow_repo_;
  }

  std::shared_ptr<minifi::core::ContentRepository> getContentRepo() {
    return content_repo_;
  }

  std::shared_ptr<minifi::core::Repository> getProvenanceRepo() {
    return prov_repo_;
  }

  [[nodiscard]] std::shared_ptr<logging::Logger> getLogger() const {
    return logger_;
  }

  [[nodiscard]] std::filesystem::path getStateDir() const {
    return state_dir_->getPath();
  }

  [[nodiscard]] std::shared_ptr<core::StateStorage> getStateStorage() const {
    return state_storage_;
  }

  std::string getContent(const std::shared_ptr<const minifi::core::FlowFile>& file) const { return getContent(*file); }
  std::string getContent(const minifi::core::FlowFile& file) const;

  void finalize();

  void validateAnnotations() const;

 protected:
  std::unique_ptr<TempDirectory> state_dir_;

  std::unique_ptr<minifi::Connection> buildFinalConnection(const std::shared_ptr<minifi::core::Processor>& processor, bool setDest = false);

  std::shared_ptr<minifi::Configure> configuration_;

  std::shared_ptr<minifi::core::ContentRepository> content_repo_;

  std::shared_ptr<minifi::core::Repository> flow_repo_;
  std::shared_ptr<minifi::core::Repository> prov_repo_;

  std::shared_ptr<minifi::core::controller::ControllerServiceProvider> controller_services_provider_;

  std::shared_ptr<minifi::core::StateStorage> state_storage_;

  std::recursive_mutex mutex;

  std::atomic<bool> finalized;

  int location;

  std::shared_ptr<minifi::core::FlowFile> current_flowfile_;

  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;
  std::vector<std::shared_ptr<minifi::core::controller::ControllerServiceNode>> controller_service_nodes_;
  std::map<minifi::utils::Identifier, std::shared_ptr<minifi::core::Processor>> processor_mapping_;
  std::vector<std::shared_ptr<minifi::core::Processor>> processor_queue_;
  std::vector<std::shared_ptr<minifi::core::Processor>> configured_processors_;  // Do not assume ordering
  std::vector<std::shared_ptr<minifi::core::ProcessorNode>> processor_nodes_;
  std::vector<std::shared_ptr<minifi::core::ProcessContext>> processor_contexts_;
  std::vector<std::shared_ptr<minifi::core::ProcessSession>> process_sessions_;
  std::vector<std::shared_ptr<minifi::core::ProcessSessionFactory>> factories_;  // Do not assume ordering
  std::vector<std::unique_ptr<minifi::Connection>> relationships_;
  std::optional<minifi::core::Relationship> termination_;

 private:
  bool setProperty(const std::shared_ptr<minifi::core::Processor>& processor, const std::string& property, const std::string& value, bool dynamic);
  static bool setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, const std::string& property, const std::string& value, bool dynamic);

  std::shared_ptr<logging::Logger> logger_;
};

class TestController {
 public:
  struct PlanConfig {
    std::shared_ptr<minifi::Configure> configuration = {};
    std::optional<std::filesystem::path> state_dir = {};
    std::shared_ptr<minifi::core::ContentRepository> content_repo = {};
    std::shared_ptr<minifi::core::Repository> flow_file_repo = {};
  };

  TestController();

  std::shared_ptr<TestPlan> createPlan(PlanConfig config);

  std::shared_ptr<TestPlan> createPlan(std::shared_ptr<minifi::Configure> configuration = nullptr, std::optional<std::filesystem::path> state_dir = {},
      std::shared_ptr<minifi::core::ContentRepository> content_repo = nullptr);

  static void runSession(const std::shared_ptr<TestPlan> &plan,
                  bool runToCompletion = true,
                  const TestPlan::PreTriggerVerifier& verify = nullptr) {
    while (plan->runNextProcessor(verify) && runToCompletion) {
    }
  }

  [[nodiscard]] const std::shared_ptr<logging::Logger>& getLogger() const {
    return log.logger_;
  }

  [[nodiscard]] const LogTestController& getLog() const {
    return log;
  }

  std::filesystem::path createTempDirectory();

 protected:
  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;
  LogTestController &log;
  std::vector<std::unique_ptr<TempDirectory>> directories;
};

static bool disableAwsMetadata = [] {
  // Disable retrieving AWS metadata for tests
#ifdef WIN32
  _putenv_s("AWS_EC2_METADATA_DISABLED", "true");
#else
  setenv("AWS_EC2_METADATA_DISABLED", "true", 1);
#endif
  return true;
}();

#if defined(LOAD_EXTENSIONS) && !defined(CUSTOM_EXTENSION_INIT)
static bool extensionInitializer = [] {
  LogTestController::getInstance().setTrace<core::extension::ExtensionManager>();
  LogTestController::getInstance().setTrace<core::extension::Module>();
  auto config = minifi::Configure::create();
#ifdef EXTENSION_LIST
  config->set(minifi::Configuration::nifi_extension_path, EXTENSION_LIST);
#else
  config->set(minifi::Configuration::nifi_extension_path, "*minifi-*");
#endif
  core::extension::ExtensionManagerImpl::get().initialize(config);
  return true;
}();
#endif

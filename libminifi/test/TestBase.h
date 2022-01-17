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

#include <chrono>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Catch.h"
#include "spdlog/common.h"

#include "core/Core.h"
#include "core/extension/ExtensionManager.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/LoggerProperties.h"
#include "core/Relationship.h"
#include "core/repository/VolatileContentRepository.h"
#include "utils/file/FileUtils.h"

namespace minifi = org::apache::nifi::minifi;
namespace utils = minifi::utils;
namespace logging = minifi::core::logging;
namespace core = minifi::core;

namespace org::apache::nifi::minifi {
class Connection;
namespace core {
class CoreComponentStateManagerProvider;
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
class ControllerServiceMap;
class ControllerServiceProvider;
}  // namespace controller
}  // namespace core
namespace state::response {
class FlowVersion;
}  // namespace state::response
namespace provenance {
class ProvenanceEventRecord;
}  // namespace provenance
namespace io {
class StreamFactory;
}  // namespace io
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
  std::shared_ptr<logging::Logger> getLogger() { return getLoggerByClassName(minifi::core::getClassName<T>()); }

  std::shared_ptr<logging::Logger> getLoggerByClassName(const std::string& class_name);

  template<typename T>
  void setLevel(spdlog::level::level_enum level) {
    setLevelByClassName(level, minifi::core::getClassName<T>());
  }

  void setLevelByClassName(spdlog::level::level_enum level, const std::string& class_name);

  bool contains(const std::string &ending, std::chrono::seconds timeout = std::chrono::seconds(3), std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200)) {
    return contains(log_output, ending, timeout, sleep_interval);
  }

  bool contains(const std::ostringstream &stream, const std::string &ending,
                std::chrono::seconds timeout = std::chrono::seconds(3),
                std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200));

  std::optional<std::smatch> matchesRegex(const std::string &regex_str,
                std::chrono::seconds timeout = std::chrono::seconds(3),
                std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(200));

  int countOccurrences(const std::string& pattern) const;

  void reset();

  void resetStream(std::ostringstream &stream);

  std::shared_ptr<std::ostringstream> log_output_ptr = std::make_shared<std::ostringstream>();
  std::ostringstream& log_output = *log_output_ptr;

  std::shared_ptr<logging::Logger> logger_;

 protected:
  LogTestController()
      : LogTestController(nullptr) {
  }

  explicit LogTestController(const std::shared_ptr<logging::LoggerProperties> &loggerProps);

  void setLevel(const std::string& name, spdlog::level::level_enum level);

  std::shared_ptr<logging::LoggerProperties> my_properties_;
  std::unique_ptr<logging::LoggerConfiguration> config;
  std::vector<std::string> modified_loggers;
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

  std::shared_ptr<minifi::Connection> addConnection(const std::shared_ptr<minifi::core::Processor>& source_proc, const minifi::core::Relationship& source_relationship, const std::shared_ptr<minifi::core::Processor>& destination_proc); // NOLINT

  std::shared_ptr<minifi::core::controller::ControllerServiceNode> addController(const std::string &controller_name, const std::string &name);

  bool setProperty(const std::shared_ptr<minifi::core::Processor>& proc, const std::string &prop, const std::string &value, bool dynamic = false);

  static bool setProperty(const std::shared_ptr<minifi::core::controller::ControllerServiceNode>& controller_service_node, const std::string &prop, const std::string &value, bool dynamic = false);

  void reset(bool reschedule = false);
  void increment_location() { ++location; }
  void reset_location() { location = -1; }

  using PreTriggerVerifier = std::function<void(const std::shared_ptr<minifi::core::ProcessContext>, const std::shared_ptr<minifi::core::ProcessSession>)>;

  std::vector<std::shared_ptr<minifi::core::Processor>>::iterator getProcessorItByUuid(const std::string& uuid);
  std::shared_ptr<minifi::core::ProcessContext> getProcessContextForProcessor(const std::shared_ptr<minifi::core::Processor>& processor);

  void scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const std::shared_ptr<minifi::core::ProcessContext>& context);
  void scheduleProcessor(const std::shared_ptr<minifi::core::Processor>& processor);
  void scheduleProcessors();

  // Note: all this verify logic is only used in TensorFlow tests as a replacement for UpdateAttribute
  // It should probably not be the part of the standard way of running processors
  bool runProcessor(const std::shared_ptr<minifi::core::Processor>& processor, const PreTriggerVerifier& verify = nullptr);
  bool runProcessor(size_t target_location, const PreTriggerVerifier& verify = nullptr);
  bool runNextProcessor(const PreTriggerVerifier& verify = nullptr);
  bool runCurrentProcessor(const PreTriggerVerifier& verify = nullptr);
  bool runCurrentProcessorUntilFlowfileIsProduced(const std::chrono::seconds& wait_duration);

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

  [[nodiscard]] std::string getStateDir() const {
    return state_dir_->getPath();
  }

  [[nodiscard]] std::shared_ptr<core::CoreComponentStateManagerProvider> getStateManagerProvider() const {
    return state_manager_provider_;
  }

  std::string getContent(const std::shared_ptr<const minifi::core::FlowFile>& file) const { return getContent(*file); }
  std::string getContent(const minifi::core::FlowFile& file) const;

  void finalize();

  void validateAnnotations() const;

 protected:
  class StateDir {
   public:
    StateDir() {
      char state_dir_name_template[] = "/var/tmp/teststate.XXXXXX";
      path_ = minifi::utils::file::FileUtils::create_temp_directory(state_dir_name_template);
      is_owner_ = true;
    }

    explicit StateDir(std::string path) : path_(std::move(path)), is_owner_(false) {}

    StateDir(const StateDir&) = delete;
    StateDir& operator=(const StateDir&) = delete;

    ~StateDir() {
      if (is_owner_) {
        minifi::utils::file::FileUtils::delete_dir(path_, true);
      }
    }

    [[nodiscard]] std::string getPath() const {
      return path_;
    }

   private:
    std::string path_;
    bool is_owner_;
  };

  std::unique_ptr<StateDir> state_dir_;

  std::shared_ptr<minifi::Connection> buildFinalConnection(const std::shared_ptr<minifi::core::Processor>& processor, bool setDest = false);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory;

  std::shared_ptr<minifi::Configure> configuration_;

  std::shared_ptr<minifi::core::ContentRepository> content_repo_;

  std::shared_ptr<minifi::core::Repository> flow_repo_;
  std::shared_ptr<minifi::core::Repository> prov_repo_;

  std::shared_ptr<minifi::core::controller::ControllerServiceMap> controller_services_;
  std::shared_ptr<minifi::core::controller::ControllerServiceProvider> controller_services_provider_;

  std::shared_ptr<minifi::core::CoreComponentStateManagerProvider> state_manager_provider_;

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
  std::vector<std::shared_ptr<minifi::Connection>> relationships_;
  minifi::core::Relationship termination_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

class TestController {
 public:
  TestController();

  std::shared_ptr<TestPlan> createPlan(std::shared_ptr<minifi::Configure> configuration = nullptr, const char* state_dir = nullptr,
      std::shared_ptr<minifi::core::ContentRepository> content_repo = std::make_shared<minifi::core::repository::VolatileContentRepository>());

  static void runSession(const std::shared_ptr<TestPlan> &plan,
                  bool runToCompletion = true,
                  const std::function<void(const std::shared_ptr<minifi::core::ProcessContext>&,
                  const std::shared_ptr<minifi::core::ProcessSession>&)>& verify = nullptr) {
    while (plan->runNextProcessor(verify) && runToCompletion) {
    }
  }

  [[nodiscard]] const std::shared_ptr<logging::Logger>& getLogger() const {
    return log.logger_;
  }

  [[nodiscard]] const LogTestController& getLog() const {
    return log;
  }

  ~TestController() {
    for (const auto& dir : directories) {
      minifi::utils::file::FileUtils::delete_dir(dir, true);
    }
  }

  std::string createTempDirectory();

 protected:
  std::shared_ptr<minifi::state::response::FlowVersion> flow_version_;
  LogTestController &log;
  std::vector<std::string> directories;
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
  auto config = std::make_shared<minifi::Configure>();
#ifdef EXTENSION_LIST
  config->set(core::extension::nifi_extension_path, EXTENSION_LIST);
#else
  config->set(core::extension::nifi_extension_path, "*minifi-*");
#endif
  core::extension::ExtensionManager::get().initialize(config);
  return true;
}();
#endif

namespace Catch {
template<typename T>
struct StringMaker<std::optional<T>> {
  static std::string convert(const std::optional<T>& val) {
    if (val) {
      return "std::optional(" + StringMaker<T>::convert(val.value()) + ")";
    }
    return "std::nullopt";
  }
};

template<>
struct StringMaker<std::nullopt_t> {
  static std::string convert(const std::nullopt_t& /*val*/) {
    return "std::nullopt";
  }
};
}  // namespace Catch

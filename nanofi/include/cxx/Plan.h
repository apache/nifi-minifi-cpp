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

#ifndef LIBMINIFI_CAPI_PLAN_H_
#define LIBMINIFI_CAPI_PLAN_H_

#ifndef WIN32
	#include <dirent.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "ResourceClaim.h"
#include <vector>
#include <set>
#include <map>
#include "core/logging/Logger.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "properties/Properties.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "api/nanofi.h"

static const std::string CallbackProcessorName = "CallbackProcessor";

using failure_callback_type = std::function<void(flow_file_record*)>;
using content_repo_sptr = std::shared_ptr<core::ContentRepository>;

struct flowfile_input_params {
  std::shared_ptr<minifi::io::DataStream> content_stream;
  std::map<std::string, std::string> attributes;
};

namespace {

  void failureStrategyAsIs(core::ProcessSession *session, failure_callback_type user_callback, content_repo_sptr cr_ptr) {
    auto ff = session->get();
    if (ff == nullptr) {
      return;
    }

    auto claim = ff->getResourceClaim();

    if (claim != nullptr && user_callback != nullptr) {
      claim->increaseFlowFileRecordOwnedCount();
      // create a flow file.
      auto path = claim->getContentFullPath();
      auto ffr = create_ff_object_na(path.c_str(), path.length(), ff->getSize());
      ffr->attributes = ff->getAttributesPtr();
      ffr->ffp = static_cast<void*>(new std::shared_ptr<minifi::core::FlowFile>(ff));
      auto content_repo_ptr = static_cast<std::shared_ptr<minifi::core::ContentRepository>*>(ffr->crp);
      *content_repo_ptr = cr_ptr;
      user_callback(ffr);
    }
    session->remove(ff);
  }

  void failureStrategyRollback(core::ProcessSession *session, failure_callback_type user_callback, content_repo_sptr cr_ptr) {
    session->rollback();
    failureStrategyAsIs(session, user_callback, cr_ptr);
  }
}

static const std::map<FailureStrategy, const std::function<void(core::ProcessSession*, failure_callback_type, content_repo_sptr)>> FailureStrategies =
    { { FailureStrategy::AS_IS, failureStrategyAsIs }, {FailureStrategy::ROLLBACK, failureStrategyRollback } };

class ExecutionPlan {
 public:

  explicit ExecutionPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo);

  std::shared_ptr<core::Processor> addSimpleCallback(void *, std::function<void(core::ProcessSession*)>);

  std::shared_ptr<core::Processor> addCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *)> fp);

  std::shared_ptr<core::Processor> addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name,
                                                core::Relationship relationship = core::Relationship("success", "description"),
                                                bool linkToPrevious = false);

  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const std::string &name, core::Relationship relationship = core::Relationship("success", "description"),
  bool linkToPrevious = false);

  bool setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value);

  void reset();

  bool runNextProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify = nullptr, std::shared_ptr<flowfile_input_params> = nullptr);

  bool setFailureCallback(failure_callback_type onerror_callback);

  bool setFailureStrategy(FailureStrategy start);

  std::set<std::shared_ptr<provenance::ProvenanceEventRecord>> getProvenanceRecords();

  std::shared_ptr<core::FlowFile> getCurrentFlowFile();

  std::shared_ptr<core::ProcessSession> getCurrentSession();

  std::shared_ptr<core::Repository> getFlowRepo() {
    return flow_repo_;
  }

  std::shared_ptr<core::Repository> getProvenanceRepo() {
    return prov_repo_;
  }

  std::shared_ptr<core::ContentRepository> getContentRepo() {
    return content_repo_;
  }

  std::shared_ptr<core::FlowFile> getNextFlowFile(){
    return next_ff_;
  }

  void setNextFlowFile(std::shared_ptr<core::FlowFile> ptr){
    next_ff_ = ptr;
  }

  bool hasProcessor() {
    return !processor_queue_.empty();
  }

  static std::shared_ptr<core::Processor> createProcessor(const std::string &processor_name, const std::string &name);

  static std::shared_ptr<core::Processor> createCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *)> fp);

  static std::shared_ptr<ExecutionPlan> getPlan(const std::string& uuid) {
    auto it = proc_plan_map_.find(uuid);
    return it != proc_plan_map_.end() ? it->second : nullptr;
  }

  static void addProcessorWithPlan(const std::string &uuid, std::shared_ptr<ExecutionPlan> plan) {
    proc_plan_map_[uuid] = plan;
  }

  static bool removeProcWithPlan(const std::string& uuid) {
    return proc_plan_map_.erase(uuid) > 0;
  }

  static size_t getProcWithPlanQty() {
    return proc_plan_map_.size();
  }

  static bool addCustomProcessor(const char * name, processor_logic* logic);

  static int deleteCustomProcessor(const char * name);

 protected:
  class FailureHandler {
   public:
    FailureHandler(content_repo_sptr cr_ptr) {
      callback_ = nullptr;
      strategy_ = FailureStrategy::AS_IS;
      content_repo_ = cr_ptr;
    }
    void setCallback(failure_callback_type onerror_callback) {
      callback_=onerror_callback;
    }
    void setStrategy(FailureStrategy strat) {
      strategy_ = strat;
    }
    void operator()(core::ProcessSession* ps) {
      FailureStrategies.at(strategy_)(ps, callback_, content_repo_);
    }
   private:
    failure_callback_type callback_;
    FailureStrategy strategy_;
    content_repo_sptr content_repo_;
  };

  void finalize();

  std::shared_ptr<minifi::Connection> buildFinalConnection(std::shared_ptr<core::Processor> processor, bool set_dst = false);

  std::shared_ptr<minifi::Connection> connectProcessors(std::shared_ptr<core::Processor> src_proc, std::shared_ptr<core::Processor> dst_proc,
                                                        core::Relationship relationship = core::Relationship("success", "description"), bool set_dst = false);

  std::shared_ptr<org::apache::nifi::minifi::io::StreamFactory> stream_factory;

  content_repo_sptr content_repo_;

  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::Repository> prov_repo_;

  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider_;

  std::atomic<bool> finalized;

  uint32_t location;

  std::shared_ptr<core::ProcessSession> current_session_;
  std::shared_ptr<core::FlowFile> current_flowfile_;

  std::map<std::string, std::shared_ptr<core::Processor>> processor_mapping_;
  std::vector<std::shared_ptr<core::Processor>> processor_queue_;
  std::vector<std::shared_ptr<core::Processor>> configured_processors_;
  std::vector<std::shared_ptr<core::ProcessorNode>> processor_nodes_;
  std::vector<std::shared_ptr<core::ProcessContext>> processor_contexts_;
  std::vector<std::shared_ptr<core::ProcessSession>> process_sessions_;
  std::vector<std::shared_ptr<core::ProcessSessionFactory>> factories_;
  std::vector<std::shared_ptr<minifi::Connection>> relationships_;
  core::Relationship termination_;

  std::shared_ptr<core::FlowFile> next_ff_;

 private:

  static std::shared_ptr<utils::IdGenerator> id_generator_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<FailureHandler> failure_handler_;
  static std::unordered_map<std::string, std::shared_ptr<ExecutionPlan>> proc_plan_map_;
  static std::map<std::string, processor_logic*> custom_processors;
};

#endif /* LIBMINIFI_CAPI_PLAN_H_ */

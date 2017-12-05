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
#include <dirent.h>
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


class ExecutionPlan {
 public:

  explicit ExecutionPlan(std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::Repository> prov_repo);

  std::shared_ptr<core::Processor> addProcessor(const std::shared_ptr<core::Processor> &processor, const std::string &name,
                                                core::Relationship relationship = core::Relationship("success", "description"),
                                                bool linkToPrevious = false);

  std::shared_ptr<core::Processor> addProcessor(const std::string &processor_name, const std::string &name, core::Relationship relationship = core::Relationship("success", "description"),
  bool linkToPrevious = false);

  bool setProperty(const std::shared_ptr<core::Processor> proc, const std::string &prop, const std::string &value);

  void reset();

  bool runNextProcessor(std::function<void(const std::shared_ptr<core::ProcessContext>, const std::shared_ptr<core::ProcessSession>)> verify = nullptr);

  std::set<provenance::ProvenanceEventRecord*> getProvenanceRecords();

  std::shared_ptr<core::FlowFile> getCurrentFlowFile();

  std::shared_ptr<core::Repository> getFlowRepo() {
    return flow_repo_;
  }

  std::shared_ptr<core::Repository> getProvenanceRepo() {
    return prov_repo_;
  }

  std::shared_ptr<core::ContentRepository> getContentRepo() {
    return content_repo_;
  }

 protected:

  void finalize();

  std::shared_ptr<minifi::Connection> buildFinalConnection(std::shared_ptr<core::Processor> processor, bool setDest = false);

  std::shared_ptr<org::apache::nifi::minifi::io::StreamFactory> stream_factory;

  std::shared_ptr<core::ContentRepository> content_repo_;

  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::Repository> prov_repo_;

  std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider_;

  std::atomic<bool> finalized;

  int location;

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

 private:

  std::shared_ptr<logging::Logger> logger_;
};

#endif /* LIBMINIFI_CAPI_PLAN_H_ */

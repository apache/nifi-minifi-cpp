/**
 * @file SiteToSiteProvenanceReportingTask.h
 * SiteToSiteProvenanceReportingTask class declaration
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
#ifndef __SITE_TO_SITE_PROVENANCE_REPORTING_TASK_H__
#define __SITE_TO_SITE_PROVENANCE_REPORTING_TASK_H__ 

#include <mutex>
#include <memory>
#include <stack>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "RemoteProcessorGroupPort.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace reporting {

//! SiteToSiteProvenanceReportingTask Class
class SiteToSiteProvenanceReportingTask : public minifi::RemoteProcessorGroupPort {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  SiteToSiteProvenanceReportingTask(const std::shared_ptr<io::StreamFactory> &stream_factory, std::shared_ptr<Configure> configure)
      : minifi::RemoteProcessorGroupPort(stream_factory, ReportTaskName, "", configure),
        logger_(logging::LoggerFactory<SiteToSiteProvenanceReportingTask>::getLogger()) {
    this->setTriggerWhenEmpty(true);
    batch_size_ = 100;
  }
  //! Destructor
  ~SiteToSiteProvenanceReportingTask() {
  }
  //! Report Task Name
  static constexpr char const* ReportTaskName = "SiteToSiteProvenanceReportingTask";
  static const char *ProvenanceAppStr;

 public:
  //! Get provenance json report
  void getJsonReport(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session, std::vector<std::shared_ptr<core::SerializableComponent>> &records, std::string &report);


  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory);
  //! OnTrigger method, implemented by NiFi SiteToSiteProvenanceReportingTask
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  //! Initialize, over write by NiFi SiteToSiteProvenanceReportingTask
  virtual void initialize(void);
  //! Set Port UUID
  void setPortUUID(utils::Identifier &port_uuid) {
    protocol_uuid_ = port_uuid;
  }

  //! Set Batch Size
  void setBatchSize(int size) {
    batch_size_ = size;
  }
  //! Get Batch Size
  int getBatchSize(void) {
    return (batch_size_);
  }
  //! Get Port UUID
  void getPortUUID(utils::Identifier & port_uuid) {
    port_uuid = protocol_uuid_;
  }

 protected:

 private:
  int batch_size_;

  std::shared_ptr<logging::Logger> logger_;
};

// SiteToSiteProvenanceReportingTask 

} /* namespace reporting */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

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
#include "Site2SiteClientProtocol.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace reporting {

//! SiteToSiteProvenanceReportingTask Class
class SiteToSiteProvenanceReportingTask :
    public minifi::RemoteProcessorGroupPort {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  SiteToSiteProvenanceReportingTask(
      const std::shared_ptr<io::StreamFactory> &stream_factory)
      : minifi::RemoteProcessorGroupPort(stream_factory, ReportTaskName),
        logger_(logging::LoggerFactory<SiteToSiteProvenanceReportingTask>::getLogger()) {
    this->setTriggerWhenEmpty(true);
    port_ = 0;
    batch_size_ = 100;
  }
  //! Destructor
  virtual ~SiteToSiteProvenanceReportingTask() {

  }
  //! Report Task Name
  static constexpr char const* ReportTaskName =
      "SiteToSiteProvenanceReportingTask";
  static const char *ProvenanceAppStr;

 public:
  //! Get provenance json report
  void getJsonReport(
      core::ProcessContext *context, core::ProcessSession *session,
      std::vector<std::shared_ptr<provenance::ProvenanceEventRecord>> &records,
      std::string &report);
  void onSchedule(core::ProcessContext *context,
                    core::ProcessSessionFactory *sessionFactory);
  //! OnTrigger method, implemented by NiFi SiteToSiteProvenanceReportingTask
  virtual void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session);
  //! Initialize, over write by NiFi SiteToSiteProvenanceReportingTask
  virtual void initialize(void);
  //! Set Port UUID
  void setPortUUID(uuid_t port_uuid) {
    uuid_copy(protocol_uuid_, port_uuid);
  }
  //! Set Host
  void setHost(std::string host) {
    host_ = host;
  }
  //! Set Port
  void setPort(uint16_t port) {
    port_ = port;
  }
  //! Set Batch Size
  void setBatchSize(int size) {
    batch_size_ = size;
  }
  //! Get Host
  std::string getHost(void) {
    return (host_);
  }
  //! Get Port
  uint16_t getPort(void) {
    return (port_);
  }
  //! Get Batch Size
  int getBatchSize(void) {
    return (batch_size_);
  }
  //! Get Port UUID
  void getPortUUID(uuid_t port_uuid) {
    uuid_copy(port_uuid, protocol_uuid_);
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

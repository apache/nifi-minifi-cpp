/**
 * @file GetGPS.h
 * GetGPS class declaration
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
#ifndef __GET_GPS_H__
#define __GET_GPS_H__

#include "../FlowFileRecord.h"
#include "../core/Processor.h"
#include "../core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

//! GetGPS Class
class GetGPS : public core::Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	GetGPS(std::string name, utils::Identifier uuid = utils::Identifier())
	: core::Processor(name, uuid), logger_(logging::LoggerFactory<GetGPS>::getLogger())
	{
		gpsdHost_ = "localhost";
		gpsdPort_ = "2947";
		gpsdWaitTime_ = 50000000;
	}
	//! Destructor
	virtual ~GetGPS()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static core::Property GPSDHost;
	static core::Property GPSDPort;
	static core::Property GPSDWaitTime;

	//! Supported Relationships
	static core::Relationship Success;

public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
	//! OnTrigger method, implemented by NiFi GetGPS
	virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
	//! Initialize, over write by NiFi GetGPS
	virtual void initialize(void) override;

protected:

private:
  std::string gpsdHost_;
  std::string gpsdPort_;
  int64_t gpsdWaitTime_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(GetGPS, "Obtains GPS coordinates from the GPSDHost and port.");


} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

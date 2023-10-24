/**
 * @file GetGPS.cpp
 * GetGPS class implementation
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
#include "GetGPS.h"
#include <sys/types.h>
#include <dirent.h>

#define policy_t gps_policy_t
#include <libgpsmm.h>
#undef  policy_t
#define policy_t ambiguous use gps_policy_t

#include <vector>
#include <set>
#include <string>
#include <memory>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void GetGPS::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void GetGPS::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  std::string value;

  if (context.getProperty(GPSDHost, value)) {
    gpsdHost_ = value;
  }
  if (context.getProperty(GPSDPort, value)) {
    gpsdPort_ = value;
  }
  if (context.getProperty(GPSDWaitTime, value)) {
    core::Property::StringToInt(value, gpsdWaitTime_);
  }
  logger_->log_trace("GPSD client scheduled");
}

int get_gps_status(struct gps_data_t* gps_data) {
#if defined(GPSD_API_MAJOR_VERSION) && GPSD_API_MAJOR_VERSION >= 10
  return gps_data->fix.status;
#else
  return gps_data->status;
#endif
}

void GetGPS::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  try {
    gpsmm gps_rec(gpsdHost_.c_str(), gpsdPort_.c_str());

    if (gps_rec.stream(WATCH_ENABLE | WATCH_JSON) == nullptr) {
      logger_->log_error("No GPSD running.");
      return;
    }

    while (isRunning()) {
      struct gps_data_t* gpsdata;

      if (!gps_rec.waiting(gpsdWaitTime_))
        continue;

      if ((gpsdata = gps_rec.read()) == nullptr) {
        logger_->log_error("Read error");
        return;
      } else {
        if (get_gps_status(gpsdata) > 0) {
          if (gpsdata->fix.longitude != gpsdata->fix.longitude || gpsdata->fix.altitude != gpsdata->fix.altitude) {
            logger_->log_info("No GPS fix.");
            continue;
          }

          logger_->log_debug("Longitude: {}\nLatitude: {}\nAltitude: {}\nAccuracy: {}\n\n", gpsdata->fix.latitude, gpsdata->fix.longitude, gpsdata->fix.altitude,
                             (gpsdata->fix.epx > gpsdata->fix.epy) ? gpsdata->fix.epx : gpsdata->fix.epy);

          auto flowFile = session.create();
          if (flowFile == nullptr)
            return;

          flowFile->addAttribute("gps_mode", std::to_string(gpsdata->fix.mode));
          flowFile->addAttribute("gps_ept", std::to_string(gpsdata->fix.ept));
          flowFile->addAttribute("gps_latitude", std::to_string(gpsdata->fix.latitude));
          flowFile->addAttribute("gps_epy", std::to_string(gpsdata->fix.epy));
          flowFile->addAttribute("gps_longitude", std::to_string(gpsdata->fix.longitude));
          flowFile->addAttribute("gps_epx", std::to_string(gpsdata->fix.epx));
          flowFile->addAttribute("gps_altitude", std::to_string(gpsdata->fix.altitude));
          flowFile->addAttribute("gps_epv", std::to_string(gpsdata->fix.epv));
          flowFile->addAttribute("gps_track", std::to_string(gpsdata->fix.track));
          flowFile->addAttribute("gps_epd", std::to_string(gpsdata->fix.epd));
          flowFile->addAttribute("gps_speed", std::to_string(gpsdata->fix.speed));
          flowFile->addAttribute("gps_eps", std::to_string(gpsdata->fix.eps));
          flowFile->addAttribute("gps_climb", std::to_string(gpsdata->fix.climb));
          flowFile->addAttribute("gps_epc", std::to_string(gpsdata->fix.epc));

          // Calculated Accuracy value
          flowFile->addAttribute("gps_accuracy", std::to_string((gpsdata->fix.epx > gpsdata->fix.epy) ? gpsdata->fix.epx : gpsdata->fix.epy));

          session.transfer(flowFile, Success);

          // Break the for(;;) waiting loop
          break;
        } else {
          logger_->log_info("Satellite lock has not yet been acquired");
        }
      }
    }
  } catch (std::exception &exception) {
    logger_->log_error("GetGPS Caught Exception {}", exception.what());
    throw;
  }
}

REGISTER_RESOURCE(GetGPS, Processor);

}  // namespace org::apache::nifi::minifi::processors

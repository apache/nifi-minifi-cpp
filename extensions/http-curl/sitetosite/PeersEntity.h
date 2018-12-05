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
#ifndef EXTENSIONS_HTTP_CURL_SITETOSITE_PEERSENTITY_H_
#define EXTENSIONS_HTTP_CURL_SITETOSITE_PEERSENTITY_H_

#include <string>
#include <exception>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include "sitetosite/Peer.h"
#include "utils/StringUtils.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

/**
 * Represents a peer. Contains the parser for the Peer JSON.
 */
class PeersEntity {
 public:

  static bool parse(const std::shared_ptr<logging::Logger> &logger, const std::string &entity, utils::Identifier id, std::vector<PeerStatus> &peer_statuses) {
    try {
      rapidjson::Document root;
      rapidjson::ParseResult ok = root.Parse(entity.c_str());

      if (!ok) {
          std::stringstream ss;
          ss << "Failed to parse archive lens stack from JSON string with reason: "
             << rapidjson::GetParseError_En(ok.Code())
             << " at offset " << ok.Offset();
  
          throw Exception(ExceptionType::GENERAL_EXCEPTION, ss.str());
      }

      if (root.HasMember("peers") && root["peers"].IsArray() && root["peers"].Size() > 0) {
        for (const auto &peer : root["peers"].GetArray()) {
          std::string hostname;
          int port = 0, flowFileCount = 0;
          bool secure = false;

          if (peer.HasMember("hostname") && peer["hostname"].IsString() &&
              peer.HasMember("port") && peer["port"].IsNumber()) {
            hostname = peer["hostname"].GetString();
            port = peer["port"].GetInt();
          }

          if (peer.HasMember("secure")) {
            if (peer["secure"].IsBool()) {
              secure = peer["secure"].GetBool();
            } else if (peer["secure"].IsString()) {
              std::string secureStr = peer["secure"].GetString();

              if (utils::StringUtils::equalsIgnoreCase(secureStr, "true")) {
                secure = true;
              } else if (utils::StringUtils::equalsIgnoreCase(secureStr, "false")) {
                secure = false;
              } else {
                  logger->log_error("could not parse secure string %s", secureStr);
                  throw std::exception();
              }
            } else {
              logger->log_warn("Invalid value type for secure, assuming false (rapidjson type id %i)",
                               peer["secure"].GetType());
            }
          }

          if (peer.HasMember("flowFileCount")) {
            if (peer["flowFileCount"].IsNumber()) {
              flowFileCount = peer["flowFileCount"].GetInt64();
            } else {
              logger->log_debug("Could not parse flowFileCount, so we're going to continue without it");
            }
          }

          // host name and port are required.
          if (!IsNullOrEmpty(hostname) && port > 0) {
            sitetosite::PeerStatus status(std::make_shared<sitetosite::Peer>(id, hostname, port, secure), flowFileCount, true);
            peer_statuses.push_back(std::move(status));
          } else {
            logger->log_debug("hostname empty or port is zero. hostname: %s, port: %d", hostname, port);
          }
        }
      } else {
        logger->log_debug("Peers is either not a member or is empty. String to analyze: %s", entity);
      }
      return true;
    } catch (Exception &exception) {
      logger->log_debug("Caught Exception %s", exception.what());
      return false;
    } catch (...) {
      logger->log_debug("General exception occurred");
      return false;
    }

  }
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_SITETOSITE_PEERSENTITY_H_ */

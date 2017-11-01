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

#include "json/json.h"
#include "sitetosite/Peer.h"
#include "utils/StringUtils.h"

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

  static bool parse(const std::shared_ptr<logging::Logger> &logger, const std::string &entity, uuid_t id, std::vector<PeerStatus> &peer_statuses) {

    Json::Reader reader;
    Json::Value root;
    try {
      if (reader.parse(entity, root)) {
        if (root.isMember("peers") && root["peers"].size() > 0) {
          for (const auto &peer : root["peers"]) {

            std::string hostname;
            int port = 0, flowFileCount = 0;
            bool secure = false;

            if (peer.isMember("hostname") && peer.isMember("port")) {
              hostname = peer["hostname"].asString();
              port = peer["port"].asInt();
            }
            if (peer.isMember("secure")) {
              // do not assume that secure parses incorrectly that we can continue without security
              try {
                secure = peer["secure"].asBool();
              } catch (...) {
                logger->log_debug("Could not properly parse secure, so we're going to try it as a string");
                std::string secureStr = peer["secure"].asString();
                if (utils::StringUtils::equalsIgnoreCase(secureStr,"true"))
                  secure = true;
                else if (utils::StringUtils::equalsIgnoreCase(secureStr,"false"))
                  secure = false;
                else{
                    logger->log_error("could not parse secure string %s",secureStr);
                    throw std::exception();
                }

              }
            }
            if (peer.isMember("flowFileCount")) {
              // we can assume that if flowFileCount cannot parse as an integer that we CAN continue.
              try {
                flowFileCount = peer["flowFileCount"].asInt();
              } catch (...) {
                logger->log_debug("Could not properly parse flowFileCount, so we're going to continue without it");
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
      }
      return true;
    } catch (Json::RuntimeError &er) {
      logger->log_debug("JSON runtime error occurred. Message: %s", er.what());
      return false;
    } catch (...) {
      logger->log_debug("General exception occurred");
      return false;
    }

  }

}
;

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_HTTP_CURL_SITETOSITE_PEERSENTITY_H_ */

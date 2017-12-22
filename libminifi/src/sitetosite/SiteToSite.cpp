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

#include "sitetosite/SiteToSite.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

const char *SiteToSiteRequest::RequestTypeStr[MAX_REQUEST_TYPE] = { "NEGOTIATE_FLOWFILE_CODEC", "REQUEST_PEER_LIST", "SEND_FLOWFILES", "RECEIVE_FLOWFILES", "SHUTDOWN" };

// Respond Code Context
RespondCodeContext SiteToSiteRequest::respondCodeContext[21] = { //NOLINT
    { RESERVED, "Reserved for Future Use", false },  //NOLINT
    { PROPERTIES_OK, "Properties OK", false },  //NOLINT
    { UNKNOWN_PROPERTY_NAME, "Unknown Property Name", true },  //NOLINT
    { ILLEGAL_PROPERTY_VALUE, "Illegal Property Value", true },  //NOLINT
    { MISSING_PROPERTY, "Missing Property", true },  //NOLINT
    { CONTINUE_TRANSACTION, "Continue Transaction", false },  //NOLINT
    { FINISH_TRANSACTION, "Finish Transaction", false },  //NOLINT
    { CONFIRM_TRANSACTION, "Confirm Transaction", true },  //NOLINT
    { TRANSACTION_FINISHED, "Transaction Finished", false },  //NOLINT
    { TRANSACTION_FINISHED_BUT_DESTINATION_FULL, "Transaction Finished But Destination is Full", false },  //NOLINT
    { CANCEL_TRANSACTION, "Cancel Transaction", true },  //NOLINT
    { BAD_CHECKSUM, "Bad Checksum", false },  //NOLINT
    { MORE_DATA, "More Data Exists", false },  //NOLINT
    { NO_MORE_DATA, "No More Data Exists", false },  //NOLINT
    { UNKNOWN_PORT, "Unknown Port", false },  //NOLINT
    { PORT_NOT_IN_VALID_STATE, "Port Not in a Valid State", true },  //NOLINT
    { PORTS_DESTINATION_FULL, "Port's Destination is Full", false },  //NOLINT
    { UNAUTHORIZED, "User Not Authorized", true },  //NOLINT
    { ABORT, "Abort", true },  //NOLINT
    { UNRECOGNIZED_RESPONSE_CODE, "Unrecognized Response Code", false },  //NOLINT
    { END_OF_STREAM, "End of Stream", false } };

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

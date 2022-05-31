/**
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

#include "ConvertBase.h"
#include "ConvertHeartBeat.h"
#include "ConvertJSONAck.h"
#include "ConvertUpdate.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// ConvertBase

const core::Property ConvertBase::MQTTControllerService("MQTT Controller Service", "Name of controller service that will be used for MQTT interactivity", "");
const core::Property ConvertBase::ListeningTopic("Listening Topic", "Name of topic to listen to", "");


// ConvertHeartBeat

REGISTER_RESOURCE(ConvertHeartBeat, InternalResource);


// ConvertJSONAck

REGISTER_RESOURCE(ConvertJSONAck, InternalResource);


// ConvertUpdate

core::Property ConvertUpdate::SSLContext("SSL Context Service", "The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.", "");

REGISTER_RESOURCE(ConvertUpdate, InternalResource);

}  // namespace org::apache::nifi::minifi::processors

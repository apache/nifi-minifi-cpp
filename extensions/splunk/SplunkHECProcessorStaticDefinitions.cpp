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

#include "PutSplunkHTTP.h"
#include "QuerySplunkIndexingStatus.h"
#include "SplunkHECProcessor.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::extensions::splunk {

// SplunkHECProcessor

const core::Property SplunkHECProcessor::Hostname(core::PropertyBuilder::createProperty("Hostname")
    ->withDescription("The ip address or hostname of the Splunk server.")
    ->isRequired(true)->build());

const core::Property SplunkHECProcessor::Port(core::PropertyBuilder::createProperty("Port")
    ->withDescription("The HTTP Event Collector HTTP Port Number.")
    ->withDefaultValue<int>(8088, core::StandardValidators::get().PORT_VALIDATOR)->isRequired(true)->build());

const core::Property SplunkHECProcessor::Token(core::PropertyBuilder::createProperty("Token")
    ->withDescription("HTTP Event Collector token starting with the string Splunk. For example \'Splunk 1234578-abcd-1234-abcd-1234abcd\'")
    ->isRequired(true)->build());

const core::Property SplunkHECProcessor::SplunkRequestChannel(core::PropertyBuilder::createProperty("Splunk Request Channel")
    ->withDescription("Identifier of the used request channel.")->isRequired(true)->build());

const core::Property SplunkHECProcessor::SSLContext(core::PropertyBuilder::createProperty("SSL Context Service")
    ->withDescription("The SSL Context Service used to provide client certificate "
                      "information for TLS/SSL (https) connections.")
    ->isRequired(false)->withExclusiveProperty("Hostname", "^http:.*$")
    ->asType<minifi::controllers::SSLContextService>()->build());


// PutSplunkHTTP

const core::Property PutSplunkHTTP::Source(core::PropertyBuilder::createProperty("Source")
    ->withDescription("Basic field describing the source of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::SourceType(core::PropertyBuilder::createProperty("Source Type")
    ->withDescription("Basic field describing the source type of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::Host(core::PropertyBuilder::createProperty("Host")
    ->withDescription("Basic field describing the host of the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::Index(core::PropertyBuilder::createProperty("Index")
    ->withDescription("Identifies the index where to send the event. If unspecified, the event will use the default defined in splunk.")
    ->supportsExpressionLanguage(true)->build());

const core::Property PutSplunkHTTP::ContentType(core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("The media type of the event sent to Splunk. If not set, \"mime.type\" flow file attribute will be used. "
                      "In case of neither of them is specified, this information will not be sent to the server.")
    ->supportsExpressionLanguage(true)->build());

const core::Relationship PutSplunkHTTP::Success("success", "FlowFiles that are sent successfully to the destination are sent to this relationship.");
const core::Relationship PutSplunkHTTP::Failure("failure", "FlowFiles that failed to be sent to the destination are sent to this relationship.");

REGISTER_RESOURCE(PutSplunkHTTP, Processor);


// QuerySplunkIndexingStatus

const core::Property QuerySplunkIndexingStatus::MaximumWaitingTime(core::PropertyBuilder::createProperty("Maximum Waiting Time")
    ->withDescription("The maximum time the processor tries to acquire acknowledgement confirmation for an index, from the point of registration. "
                      "After the given amount of time, the processor considers the index as not acknowledged and transfers the FlowFile to the \"unacknowledged\" relationship.")
    ->withDefaultValue<core::TimePeriodValue>("1 hour")->isRequired(true)->build());

const core::Property QuerySplunkIndexingStatus::MaxQuerySize(core::PropertyBuilder::createProperty("Maximum Query Size")
    ->withDescription("The maximum number of acknowledgement identifiers the outgoing query contains in one batch. "
                      "It is recommended not to set it too low in order to reduce network communication.")
    ->withDefaultValue<uint64_t>(1000)->isRequired(true)->build());


const core::Relationship QuerySplunkIndexingStatus::Acknowledged("acknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was successful.");

const core::Relationship QuerySplunkIndexingStatus::Unacknowledged("unacknowledged",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful. "
    "This can happen when the acknowledgement did not happened within the time period set for Maximum Waiting Time. "
    "FlowFiles with acknowledgement id unknown for the Splunk server will be transferred to this relationship after the Maximum Waiting Time is reached.");

const core::Relationship QuerySplunkIndexingStatus::Undetermined("undetermined",
    "A FlowFile is transferred to this relationship when the acknowledgement state is not determined. "
    "FlowFiles transferred to this relationship might be penalized. "
    "This happens when Splunk returns with HTTP 200 but with false response for the acknowledgement id in the flow file attribute.");

const core::Relationship QuerySplunkIndexingStatus::Failure("failure",
    "A FlowFile is transferred to this relationship when the acknowledgement was not successful due to errors during the communication, "
    "or if the flowfile was missing the acknowledgement id");

REGISTER_RESOURCE(QuerySplunkIndexingStatus, Processor);

}  // namespace org::apache::nifi::minifi::extensions::splunk

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
#include "AbstractMQTTProcessor.h"
#include "ConsumeMQTT.h"
#include "PublishMQTT.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// AbstractMQTTProcessor

const core::Property AbstractMQTTProcessor::BrokerURI(
  core::PropertyBuilder::createProperty("Broker URI")->
    withDescription("The URI to use to connect to the MQTT broker")->
    isRequired(true)->
    build());

const core::Property AbstractMQTTProcessor::ClientID(
        core::PropertyBuilder::createProperty("Client ID")->
        withDescription("MQTT client ID to use. WARNING: Must not be empty when using MQTT 3.1.0!")->
        build());

const core::Property AbstractMQTTProcessor::QoS(
        core::PropertyBuilder::createProperty("Quality of Service")->
                withDescription("The Quality of Service (QoS) of messages.")->
                withDefaultValue(toString(MqttQoS::LEVEL_0))->
                withAllowableValues(MqttQoS::values())->
                build());

const core::Property AbstractMQTTProcessor::KeepAliveInterval("Keep Alive Interval", "Defines the maximum time interval between messages sent or received", "60 sec");
const core::Property AbstractMQTTProcessor::ConnectionTimeout("Connection Timeout", "Maximum time interval the client will wait for the network connection to the MQTT broker", "10 sec");
const core::Property AbstractMQTTProcessor::Username("Username", "Username to use when connecting to the broker", "");
const core::Property AbstractMQTTProcessor::Password("Password", "Password to use when connecting to the broker", "");
const core::Property AbstractMQTTProcessor::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
const core::Property AbstractMQTTProcessor::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
const core::Property AbstractMQTTProcessor::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
const core::Property AbstractMQTTProcessor::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
const core::Property AbstractMQTTProcessor::SecurityPrivateKeyPassword("Security Pass Phrase", "Private key passphrase", "");
const core::Property AbstractMQTTProcessor::LastWillTopic("Last Will Topic", "The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent", "");
const core::Property AbstractMQTTProcessor::LastWillMessage("Last Will Message",
                                                            "The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker", "");

const core::Property AbstractMQTTProcessor::LastWillQoS(
        core::PropertyBuilder::createProperty("Last Will QoS")->
                withDescription("The Quality of Service (QoS) to send the last will with.")->
                withDefaultValue(toString(MqttQoS::LEVEL_0))->
                withAllowableValues(MqttQoS::values())->
                build());

const core::Property AbstractMQTTProcessor::LastWillRetain("Last Will Retain", "Whether to retain the client's Last Will", "false");
const core::Property AbstractMQTTProcessor::LastWillContentType("Last Will Content Type", "Content type of the client's Last Will. MQTT 5.x only.", "");

const core::Property AbstractMQTTProcessor::MqttVersion(
        core::PropertyBuilder::createProperty("MQTT Version")->
                withDescription("The MQTT specification version when connecting to the broker.")->
                withDefaultValue(toString(MqttVersions::V_3X_AUTO))->
                withAllowableValues(MqttVersions::values())->
                build());

// ConsumeMQTT

const core::Property ConsumeMQTT::Topic(
        core::PropertyBuilder::createProperty("Topic")->
                withDescription("The topic to subscribe to.")->
                isRequired(true)->
                build());

const core::Property ConsumeMQTT::CleanSession("Clean Session", "Whether to start afresh rather than remembering previous subscriptions. "
                                                                "If true, then make broker forget subscriptions after disconnected. MQTT 3.x only.", "true");
const core::Property ConsumeMQTT::CleanStart("Clean Start", "Whether to start afresh rather than remembering previous subscriptions. MQTT 5.x only.", "true");
const core::Property ConsumeMQTT::SessionExpiryInterval("Session Expiry Interval", "Time to delete session on broker after client is disconnected. MQTT 5.x only.", "0 s");
const core::Property ConsumeMQTT::QueueBufferMaxMessage("Queue Max Message", "Maximum number of messages allowed on the received MQTT queue", "1000");
const core::Property ConsumeMQTT::AttributeFromContentType("Attribute From Content Type", "Name of FlowFile attribute to be filled from content type of received message. MQTT 5.x only.", "");
const core::Property ConsumeMQTT::TopicAliasMaximum("Topic Alias Maximum", "Maximum number of topic aliases to use. If set to 0, then topic aliases cannot be used. MQTT 5.x only.", "0");
const core::Property ConsumeMQTT::ReceiveMaximum("Receive Maximum", "Maximum number of unacknowledged messages allowed. MQTT 5.x only.", std::to_string(MQTT_MAX_RECEIVE_MAXIMUM));

const core::Relationship ConsumeMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");

REGISTER_RESOURCE(ConsumeMQTT, Processor);


// PublishMQTT

const core::Property PublishMQTT::Topic(
        core::PropertyBuilder::createProperty("Topic")->
                withDescription("The topic to publish to.")->
                isRequired(true)->
                supportsExpressionLanguage(true)->
                build());

const core::Property PublishMQTT::Retain("Retain", "Retain published message in broker", "false");
const core::Property PublishMQTT::MessageExpiryInterval("Message Expiry Interval", "Time while message is valid and will be forwarded by broker. MQTT 5.x only.", "");

const core::Property PublishMQTT::ContentType(
        core::PropertyBuilder::createProperty("Content Type")->
                withDescription("Content type of the message. MQTT 5.x only.")->
                supportsExpressionLanguage(true)->
                build());

const core::Relationship PublishMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");
const core::Relationship PublishMQTT::Failure("failure", "FlowFiles that failed to be sent to the destination are transferred to this relationship");

REGISTER_RESOURCE(PublishMQTT, Processor);

}  // namespace org::apache::nifi::minifi::processors

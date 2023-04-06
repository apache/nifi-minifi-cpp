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

#include "ConsumeKafka.h"
#include "KafkaProcessorBase.h"
#include "PublishKafka.h"
#include "controllers/SSLContextService.h"
#include "core/PropertyBuilder.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// KafkaProcessorBase

const core::Property KafkaProcessorBase::SecurityProtocol(
        core::PropertyBuilder::createProperty("Security Protocol")
        ->withDescription("Protocol used to communicate with brokers. Corresponds to Kafka's 'security.protocol' property.")
        ->withDefaultValue<std::string>(toString(SecurityProtocolOption::PLAINTEXT))
        ->withAllowableValues<std::string>(SecurityProtocolOption::values())
        ->isRequired(true)
        ->build());
const core::Property KafkaProcessorBase::SSLContextService(
    core::PropertyBuilder::createProperty("SSL Context Service")
        ->withDescription("SSL Context Service Name")
        ->asType<minifi::controllers::SSLContextService>()
        ->build());
const core::Property KafkaProcessorBase::KerberosServiceName(
    core::PropertyBuilder::createProperty("Kerberos Service Name")
        ->withDescription("Kerberos Service Name")
        ->build());
const core::Property KafkaProcessorBase::KerberosPrincipal(
    core::PropertyBuilder::createProperty("Kerberos Principal")
        ->withDescription("Keberos Principal")
        ->build());
const core::Property KafkaProcessorBase::KerberosKeytabPath(
    core::PropertyBuilder::createProperty("Kerberos Keytab Path")
        ->withDescription("The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.")
        ->build());
const core::Property KafkaProcessorBase::SASLMechanism(
        core::PropertyBuilder::createProperty("SASL Mechanism")
        ->withDescription("The SASL mechanism to use for authentication. Corresponds to Kafka's 'sasl.mechanism' property.")
        ->withDefaultValue<std::string>(toString(SASLMechanismOption::GSSAPI))
        ->withAllowableValues<std::string>(SASLMechanismOption::values())
        ->isRequired(true)
        ->build());
const core::Property KafkaProcessorBase::Username(
    core::PropertyBuilder::createProperty("Username")
        ->withDescription("The username when the SASL Mechanism is sasl_plaintext")
        ->build());
const core::Property KafkaProcessorBase::Password(
    core::PropertyBuilder::createProperty("Password")
        ->withDescription("The password for the given username when the SASL Mechanism is sasl_plaintext")
        ->build());


// ConsumeKafka

const core::Property ConsumeKafka::KafkaBrokers(core::PropertyBuilder::createProperty("Kafka Brokers")
  ->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>.")
  ->withDefaultValue("localhost:9092", core::StandardValidators::NON_BLANK_VALIDATOR)
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::TopicNames(core::PropertyBuilder::createProperty("Topic Names")
  ->withDescription("The name of the Kafka Topic(s) to pull from. Multiple topic names are supported as a comma separated list.")
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::TopicNameFormat(core::PropertyBuilder::createProperty("Topic Name Format")
  ->withDescription("Specifies whether the Topic(s) provided are a comma separated list of names or a single regular expression. "
                    "Using regular expressions does not automatically discover Kafka topics created after the processor started.")
  ->withAllowableValues<std::string>({TOPIC_FORMAT_NAMES, TOPIC_FORMAT_PATTERNS})
  ->withDefaultValue(TOPIC_FORMAT_NAMES)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::HonorTransactions(core::PropertyBuilder::createProperty("Honor Transactions")
  ->withDescription(
      "Specifies whether or not MiNiFi should honor transactional guarantees when communicating with Kafka. If false, the Processor will use an \"isolation level\" of "
      "read_uncomitted. This means that messages will be received as soon as they are written to Kafka but will be pulled, even if the producer cancels the transactions. "
      "If this value is true, MiNiFi will not receive any messages for which the producer's transaction was canceled, but this can result in some latency since the consumer "
      "must wait for the producer to finish its entire transaction instead of pulling as the messages become available.")
  ->withDefaultValue<bool>(true)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::GroupID(core::PropertyBuilder::createProperty("Group ID")
  ->withDescription("A Group ID is used to identify consumers that are within the same consumer group. Corresponds to Kafka's 'group.id' property.")
  ->supportsExpressionLanguage(true)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::OffsetReset(core::PropertyBuilder::createProperty("Offset Reset")
  ->withDescription("Allows you to manage the condition when there is no initial offset in Kafka or if the current offset does not exist any more on the server (e.g. because that "
      "data has been deleted). Corresponds to Kafka's 'auto.offset.reset' property.")
  ->withAllowableValues<std::string>({OFFSET_RESET_EARLIEST, OFFSET_RESET_LATEST, OFFSET_RESET_NONE})
  ->withDefaultValue(OFFSET_RESET_LATEST)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::KeyAttributeEncoding(core::PropertyBuilder::createProperty("Key Attribute Encoding")
  ->withDescription("FlowFiles that are emitted have an attribute named 'kafka.key'. This property dictates how the value of the attribute should be encoded.")
  ->withAllowableValues<std::string>({KEY_ATTR_ENCODING_UTF_8, KEY_ATTR_ENCODING_HEX})
  ->withDefaultValue(KEY_ATTR_ENCODING_UTF_8)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::MessageDemarcator(core::PropertyBuilder::createProperty("Message Demarcator")
  ->withDescription("Since KafkaConsumer receives messages in batches, you have an option to output FlowFiles which contains all Kafka messages in a single batch "
      "for a given topic and partition and this property allows you to provide a string (interpreted as UTF-8) to use for demarcating apart multiple Kafka messages. "
      "This is an optional property and if not provided each Kafka message received will result in a single FlowFile which time it is triggered. ")
  ->supportsExpressionLanguage(true)
  ->build());

const core::Property ConsumeKafka::MessageHeaderEncoding(core::PropertyBuilder::createProperty("Message Header Encoding")
  ->withDescription("Any message header that is found on a Kafka message will be added to the outbound FlowFile as an attribute. This property indicates the Character Encoding "
      "to use for deserializing the headers.")
  ->withAllowableValues<std::string>({MSG_HEADER_ENCODING_UTF_8, MSG_HEADER_ENCODING_HEX})
  ->withDefaultValue(MSG_HEADER_ENCODING_UTF_8)
  ->build());

const core::Property ConsumeKafka::HeadersToAddAsAttributes(core::PropertyBuilder::createProperty("Headers To Add As Attributes")
  ->withDescription("A comma separated list to match against all message headers. Any message header whose name matches an item from the list will be added to the FlowFile "
      "as an Attribute. If not specified, no Header values will be added as FlowFile attributes. The behaviour on when multiple headers of the same name are present is set using "
      "the Duplicate Header Handling attribute.")
  ->build());

const core::Property ConsumeKafka::DuplicateHeaderHandling(core::PropertyBuilder::createProperty("Duplicate Header Handling")
  ->withDescription("For headers to be added as attributes, this option specifies how to handle cases where multiple headers are present with the same key. "
      "For example in case of receiving these two headers: \"Accept: text/html\" and \"Accept: application/xml\" and we want to attach the value of \"Accept\" "
      "as a FlowFile attribute:\n"
      " - \"Keep First\" attaches: \"Accept -> text/html\"\n"
      " - \"Keep Latest\" attaches: \"Accept -> application/xml\"\n"
      " - \"Comma-separated Merge\" attaches: \"Accept -> text/html, application/xml\"\n")
  ->withAllowableValues<std::string>({MSG_HEADER_KEEP_FIRST, MSG_HEADER_KEEP_LATEST, MSG_HEADER_COMMA_SEPARATED_MERGE})
  ->withDefaultValue(MSG_HEADER_KEEP_LATEST)  // Mirroring NiFi behaviour
  ->build());

const core::Property ConsumeKafka::MaxPollRecords(core::PropertyBuilder::createProperty("Max Poll Records")
  ->withDescription("Specifies the maximum number of records Kafka should return when polling each time the processor is triggered.")
  ->withDefaultValue<unsigned int>(DEFAULT_MAX_POLL_RECORDS)
  ->build());

constexpr core::ConsumeKafkaMaxPollTimeValidator CONSUME_KAFKA_MAX_POLL_TIME_VALIDATOR;

const core::Property ConsumeKafka::MaxPollTime(core::PropertyBuilder::createProperty("Max Poll Time")
  ->withDescription("Specifies the maximum amount of time the consumer can use for polling data from the brokers. "
      "Polling is a blocking operation, so the upper limit of this value is specified in 4 seconds.")
  ->withDefaultValue(DEFAULT_MAX_POLL_TIME, CONSUME_KAFKA_MAX_POLL_TIME_VALIDATOR)
  ->isRequired(true)
  ->build());

const core::Property ConsumeKafka::SessionTimeout(core::PropertyBuilder::createProperty("Session Timeout")
  ->withDescription("Client group session and failure detection timeout. The consumer sends periodic heartbeats "
      "to indicate its liveness to the broker. If no hearts are received by the broker for a group member within "
      "the session timeout, the broker will remove the consumer from the group and trigger a rebalance. "
      "The allowed range is configured with the broker configuration properties group.min.session.timeout.ms and group.max.session.timeout.ms.")
  ->withDefaultValue<core::TimePeriodValue>("60 seconds")
  ->build());

const core::Relationship ConsumeKafka::Success("success", "Incoming Kafka messages as flowfiles. Depending on the demarcation strategy, this can be one or multiple flowfiles per message.");

REGISTER_RESOURCE(ConsumeKafka, Processor);


// PublishKafka

const core::Property PublishKafka::SeedBrokers(
    core::PropertyBuilder::createProperty("Known Brokers")->withDescription("A comma-separated list of known Kafka Brokers in the format <host>:<port>")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
const core::Property PublishKafka::Topic(
    core::PropertyBuilder::createProperty("Topic Name")->withDescription("The Kafka Topic of interest")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::DeliveryGuarantee(
    core::PropertyBuilder::createProperty("Delivery Guarantee")->withDescription("Specifies the requirement for guaranteeing that a message is sent to Kafka. "
                                                                                 "Valid values are 0 (do not wait for acks), "
                                                                                 "-1 or all (block until message is committed by all in sync replicas) "
                                                                                 "or any concrete number of nodes.")
        ->isRequired(false)->supportsExpressionLanguage(true)->withDefaultValue(DELIVERY_ONE_NODE)->build());
const core::Property PublishKafka::MaxMessageSize(
    core::PropertyBuilder::createProperty("Max Request Size")->withDescription("Maximum Kafka protocol request message size")
        ->isRequired(false)->build());

const core::Property PublishKafka::RequestTimeOut(
    core::PropertyBuilder::createProperty("Request Timeout")->withDescription("The ack timeout of the producer request")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("10 sec")->build());

const core::Property PublishKafka::MessageTimeOut(
    core::PropertyBuilder::createProperty("Message Timeout")->withDescription("The total time sending a message could take")
        ->isRequired(false)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());

const core::Property PublishKafka::ClientName(
    core::PropertyBuilder::createProperty("Client Name")->withDescription("Client Name to use when communicating with Kafka")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());

const core::Property PublishKafka::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("Maximum number of messages batched in one MessageSet")
        ->isRequired(false)->withDefaultValue<uint32_t>(10)->build());
const core::Property PublishKafka::TargetBatchPayloadSize(
    core::PropertyBuilder::createProperty("Target Batch Payload Size")->withDescription("The target total payload size for a batch. 0 B means unlimited (Batch Size is still applied).")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("512 KB")->build());
const core::Property PublishKafka::AttributeNameRegex("Attributes to Send as Headers", "Any attribute whose name matches the regex will be added to the Kafka messages as a Header", "");

const core::Property PublishKafka::QueueBufferMaxTime(
        core::PropertyBuilder::createProperty("Queue Buffering Max Time")
        ->isRequired(false)
        ->withDefaultValue<core::TimePeriodValue>("5 millis")
        ->withDescription("Delay to wait for messages in the producer queue to accumulate before constructing message batches")
        ->build());
const core::Property PublishKafka::QueueBufferMaxSize(
        core::PropertyBuilder::createProperty("Queue Max Buffer Size")
        ->isRequired(false)
        ->withDefaultValue<core::DataSizeValue>("1 MB")
        ->withDescription("Maximum total message size sum allowed on the producer queue")
        ->build());
const core::Property PublishKafka::QueueBufferMaxMessage(
        core::PropertyBuilder::createProperty("Queue Max Message")
        ->isRequired(false)
        ->withDefaultValue<uint64_t>(1000)
        ->withDescription("Maximum number of messages allowed on the producer queue")
        ->build());
const core::Property PublishKafka::CompressCodec(
        core::PropertyBuilder::createProperty("Compress Codec")
        ->isRequired(false)
        ->withDefaultValue<std::string>(COMPRESSION_CODEC_NONE)
        ->withAllowableValues<std::string>({COMPRESSION_CODEC_NONE, COMPRESSION_CODEC_GZIP, COMPRESSION_CODEC_SNAPPY})
        ->withDescription("compression codec to use for compressing message sets")
        ->build());
const core::Property PublishKafka::MaxFlowSegSize(
    core::PropertyBuilder::createProperty("Max Flow Segment Size")->withDescription("Maximum flow content payload segment size for the kafka record. 0 B means unlimited.")
        ->isRequired(false)->withDefaultValue<core::DataSizeValue>("0 B")->build());

const core::Property PublishKafka::SecurityCA("Security CA", "DEPRECATED in favor of SSL Context Service. File or directory path to CA certificate(s) for verifying the broker's key", "");
const core::Property PublishKafka::SecurityCert("Security Cert", "DEPRECATED in favor of SSL Context Service.Path to client's public key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKey("Security Private Key", "DEPRECATED in favor of SSL Context Service.Path to client's private key (PEM) used for authentication", "");
const core::Property PublishKafka::SecurityPrivateKeyPassWord("Security Pass Phrase", "DEPRECATED in favor of SSL Context Service.Private key passphrase", "");
const core::Property PublishKafka::KafkaKey(
    core::PropertyBuilder::createProperty("Kafka Key")
        ->withDescription("The key to use for the message. If not specified, the UUID of the flow file is used as the message key.")
        ->supportsExpressionLanguage(true)
        ->build());
const core::Property PublishKafka::MessageKeyField("Message Key Field", "DEPRECATED, does not work -- use Kafka Key instead", "");

const core::Property PublishKafka::DebugContexts("Debug contexts", "A comma-separated list of debug contexts to enable."
                                           "Including: generic, broker, topic, metadata, feature, queue, msg, protocol, cgrp, security, fetch, interceptor, plugin, consumer, admin, eos, all", "");
const core::Property PublishKafka::FailEmptyFlowFiles(
    core::PropertyBuilder::createProperty("Fail empty flow files")
        ->withDescription("Keep backwards compatibility with <=0.7.0 bug which caused flow files with empty content to not be published to Kafka and forwarded to failure. The old behavior is "
                          "deprecated. Use connections to drop empty flow files!")
        ->isRequired(false)
        ->withDefaultValue<bool>(true)
        ->build());

const core::Relationship PublishKafka::Success("success", "Any FlowFile that is successfully sent to Kafka will be routed to this Relationship");
const core::Relationship PublishKafka::Failure("failure", "Any FlowFile that cannot be sent to Kafka will be routed to this Relationship");

REGISTER_RESOURCE(PublishKafka, Processor);

}  // namespace org::apache::nifi::minifi::processors

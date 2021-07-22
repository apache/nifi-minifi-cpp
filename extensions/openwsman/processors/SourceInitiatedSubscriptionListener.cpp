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

#include "SourceInitiatedSubscriptionListener.h"
#include <openssl/x509.h>

#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <tuple>

extern "C" {
#include "wsman-api.h"
#include "wsman-xml-api.h"
#include "wsman-xml-serialize.h"
#include "wsman-xml-serializer.h"
#include "wsman-soap.h"
#include "wsman-soap-envelope.h"
}

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/tls/CertificateUtils.h"

#define XML_NS_CUSTOM_SUBSCRIPTION "http://schemas.microsoft.com/wbem/wsman/1/subscription"
#define XML_NS_CUSTOM_AUTHENTICATION "http://schemas.microsoft.com/wbem/wsman/1/authentication"
#define XML_NS_CUSTOM_POLICY "http://schemas.xmlsoap.org/ws/2002/12/policy"
#define XML_NS_CUSTOM_MACHINEID "http://schemas.microsoft.com/wbem/wsman/1/machineid"
#define WSMAN_CUSTOM_ACTION_ACK "http://schemas.dmtf.org/wbem/wsman/1/wsman/Ack"
#define WSMAN_CUSTOM_ACTION_HEARTBEAT "http://schemas.dmtf.org/wbem/wsman/1/wsman/Heartbeat"
#define WSMAN_CUSTOM_ACTION_EVENTS "http://schemas.dmtf.org/wbem/wsman/1/wsman/Events"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
core::Property SourceInitiatedSubscriptionListener::ListenHostname(
    core::PropertyBuilder::createProperty("Listen Hostname")->withDescription("The hostname or IP of this machine that will be advertised to event sources to connect to. "
                                                                              "It must be contained as a Subject Alternative Name in the server certificate, "
                                                                              "otherwise source machines will refuse to connect.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscriptionListener::ListenPort(
    core::PropertyBuilder::createProperty("Listen Port")->withDescription("The port to listen on.")
        ->isRequired(true)->withDefaultValue<int64_t>(5986, core::StandardValidators::get().LISTEN_PORT_VALIDATOR)->build());
core::Property SourceInitiatedSubscriptionListener::SubscriptionManagerPath(
    core::PropertyBuilder::createProperty("Subscription Manager Path")->withDescription("The URI path that will be used for the WEC Subscription Manager endpoint.")
        ->isRequired(true)->withDefaultValue("/wsman/SubscriptionManager/WEC")->build());
core::Property SourceInitiatedSubscriptionListener::SubscriptionsBasePath(
    core::PropertyBuilder::createProperty("Subscriptions Base Path")->withDescription("The URI path that will be used as the base for endpoints serving individual subscriptions.")
        ->isRequired(true)->withDefaultValue("/wsman/subscriptions")->build());
core::Property SourceInitiatedSubscriptionListener::SSLCertificate(
    core::PropertyBuilder::createProperty("SSL Certificate")->withDescription("File containing PEM-formatted file including TLS/SSL certificate and key. "
                                                                              "The root CA of the certificate must be the CA set in SSL Certificate Authority.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscriptionListener::SSLCertificateAuthority(
    core::PropertyBuilder::createProperty("SSL Certificate Authority")->withDescription("File containing the PEM-formatted CA that is the root CA for both this server's certificate "
                                                                                        "and the event source clients' certificates.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscriptionListener::SSLVerifyPeer(
    core::PropertyBuilder::createProperty("SSL Verify Peer")->withDescription("Whether or not to verify the client's certificate")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property SourceInitiatedSubscriptionListener::XPathXmlQuery(
    core::PropertyBuilder::createProperty("XPath XML Query")->withDescription("An XPath Query in structured XML format conforming to the Query Schema described in "
                                                                              "https://docs.microsoft.com/en-gb/windows/win32/wes/queryschema-schema, "
                                                                              "see an example here: https://docs.microsoft.com/en-gb/windows/win32/wes/consuming-events")
        ->isRequired(true)
        ->withDefaultValue("<QueryList>\n"
                           "  <Query Id=\"0\">\n"
                           "    <Select Path=\"Application\">*</Select>\n"
                           "  </Query>\n"
                           "</QueryList>\n")->build());
core::Property SourceInitiatedSubscriptionListener::InitialExistingEventsStrategy(
    core::PropertyBuilder::createProperty("Initial Existing Events Strategy")->withDescription("Defines the behaviour of the Processor when a new event source connects.\n"
    "None: will not request existing events\n"
    "All: will request all existing events matching the query")
        ->isRequired(true)->withAllowableValues<std::string>({INITIAL_EXISTING_EVENTS_STRATEGY_NONE, INITIAL_EXISTING_EVENTS_STRATEGY_ALL})
        ->withDefaultValue(INITIAL_EXISTING_EVENTS_STRATEGY_NONE)->build());
core::Property SourceInitiatedSubscriptionListener::SubscriptionExpirationInterval(
    core::PropertyBuilder::createProperty("Subscription Expiration Interval")->withDescription("The interval while a subscription is valid without renewal. "
    "Because in a source-initiated subscription, the collector can not cancel the subscription, "
    "setting this too large could cause unnecessary load on the source machine. "
    "Setting this too small causes frequent reenumeration and resubscription which is ineffective.")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("10 min")->build());
core::Property SourceInitiatedSubscriptionListener::HeartbeatInterval(
    core::PropertyBuilder::createProperty("Heartbeat Interval")->withDescription("The processor will ask the sources to send heartbeats with this interval.")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property SourceInitiatedSubscriptionListener::MaxElements(
    core::PropertyBuilder::createProperty("Max Elements")->withDescription("The maximum number of events a source will batch together and send at once.")
        ->isRequired(true)->withDefaultValue<uint32_t>(20U)->build());
core::Property SourceInitiatedSubscriptionListener::MaxLatency(
    core::PropertyBuilder::createProperty("Max Latency")->withDescription("The maximum time a source will wait to send new events.")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("10 sec")->build());
core::Property SourceInitiatedSubscriptionListener::ConnectionRetryInterval(
    core::PropertyBuilder::createProperty("Connection Retry Interval")->withDescription("The interval with which a source will try to reconnect to the server.")
        ->withDefaultValue<core::TimePeriodValue>("10 sec")->build());
core::Property SourceInitiatedSubscriptionListener::ConnectionRetryCount(
    core::PropertyBuilder::createProperty("Connection Retry Count")->withDescription("The number of connection retries after which a source will consider the subscription expired.")
        ->withDefaultValue<uint32_t>(5U)->build());

core::Relationship SourceInitiatedSubscriptionListener::Success("success", "All Events are routed to success");

constexpr char const* SourceInitiatedSubscriptionListener::ATTRIBUTE_WEF_REMOTE_MACHINEID;
constexpr char const* SourceInitiatedSubscriptionListener::ATTRIBUTE_WEF_REMOTE_IP;

constexpr char const* SourceInitiatedSubscriptionListener::INITIAL_EXISTING_EVENTS_STRATEGY_NONE;
constexpr char const* SourceInitiatedSubscriptionListener::INITIAL_EXISTING_EVENTS_STRATEGY_ALL;

constexpr char const* SourceInitiatedSubscriptionListener::ProcessorName;

SourceInitiatedSubscriptionListener::SourceInitiatedSubscriptionListener(const std::string& name, const utils::Identifier& uuid)
    : Processor(name, uuid)
    , logger_(logging::LoggerFactory<SourceInitiatedSubscriptionListener>::getLogger())
    , session_factory_(nullptr)
    , listen_port_(0U)
    , subscription_expiration_interval_(0)
    , heartbeat_interval_(0)
    , max_elements_(0U)
    , max_latency_(0)
    , connection_retry_interval_(0)
    , connection_retry_count_(0U) {
}

SourceInitiatedSubscriptionListener::Handler::Handler(SourceInitiatedSubscriptionListener& processor)
    : processor_(processor) {
}

SourceInitiatedSubscriptionListener::SubscriberData::SubscriberData()
    : bookmark_(nullptr)
    , subscription_(nullptr) {
}

SourceInitiatedSubscriptionListener::SubscriberData::~SubscriberData() {
  clearSubscription();
  clearBookmark();
}

void SourceInitiatedSubscriptionListener::SubscriberData::setSubscription(const std::string& subscription_version,
    WsXmlDocH subscription,
    const std::string& subscription_endpoint,
    const std::string& subscription_identifier) {
  clearSubscription();
  subscription_version_ = subscription_version;
  subscription_ = subscription;
  subscription_endpoint_ = subscription_endpoint;
  subscription_identifier_ = subscription_identifier;
}

void SourceInitiatedSubscriptionListener::SubscriberData::clearSubscription() {
  subscription_version_.clear();
  if (subscription_ != nullptr) {
    ws_xml_destroy_doc(subscription_);
  }
  subscription_ = nullptr;
}

void SourceInitiatedSubscriptionListener::SubscriberData::setBookmark(WsXmlDocH bookmark) {
  clearBookmark();
  bookmark_ = bookmark;
}

void SourceInitiatedSubscriptionListener::SubscriberData::clearBookmark() {
  if (bookmark_ != nullptr) {
    ws_xml_destroy_doc(bookmark_);
  }
  bookmark_ = nullptr;
}

bool SourceInitiatedSubscriptionListener::persistState() const {
  std::unordered_map<std::string, std::string> state_map;
  size_t i = 0U;
  for (const auto& subscriber : subscribers_) {
    char* xml_buf = nullptr;
    int xml_buf_size = 0;
    ws_xml_dump_memory_enc(subscriber.second.bookmark_, &xml_buf, &xml_buf_size, "UTF-8");
    state_map.emplace("subscriber." + std::to_string(i) + ".machineid", subscriber.first);
    state_map.emplace("subscriber." + std::to_string(i) + ".bookmark", std::string(xml_buf, xml_buf_size));
    i++;
    ws_xml_free_memory(xml_buf);
  }
  return state_manager_->set(state_map);
}

bool SourceInitiatedSubscriptionListener::loadState() {
  std::unordered_map<std::string, std::string> state_map;
  if (!state_manager_->get(state_map)) {
    return false;
  }

  for (size_t i = 0U;; i++) {
    std::string machineId;
    try {
      machineId = state_map.at("subscriber." + std::to_string(i) + ".machineid");
    } catch (...) {
      break;
    }

    std::string bookmark;
    try {
      bookmark = state_map.at("subscriber." + std::to_string(i) + ".bookmark");
    } catch (...) {
      logger_->log_error("Bookmark for subscriber \"%s\" is missing, skipping", machineId);
      continue;
    }

    WsXmlDocH doc = ws_xml_read_memory(bookmark.data(), bookmark.size(), "UTF-8", 0);
    if (doc == nullptr) {
      logger_->log_error("Failed to parse saved bookmark for subscriber \"%s\", skipping", machineId);
      continue;
    }
    subscribers_[machineId].setBookmark(doc);
  }

  return true;
}

std::string SourceInitiatedSubscriptionListener::Handler::millisecondsToXsdDuration(int64_t milliseconds) {
  char buf[1024];
  snprintf(buf, sizeof(buf), "PT%" PRId64 ".%03" PRId64 "S", milliseconds / 1000, milliseconds % 1000);
  return buf;
}

bool SourceInitiatedSubscriptionListener::Handler::handlePost(CivetServer* /*server*/, struct mg_connection* conn) {
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    processor_.logger_->log_error("Failed to get request info");
    return false;
  }

  const char* endpoint = req_info->local_uri;
  if (endpoint == nullptr) {
    processor_.logger_->log_error("Failed to get called endpoint (local_uri)");
    return false;
  }
  processor_.logger_->log_trace("Endpoint \"%s\" has been called", endpoint);

  for (int i = 0; i < req_info->num_headers; i++) {
    processor_.logger_->log_trace("Received header \"%s: %s\"", req_info->http_headers[i].name, req_info->http_headers[i].value);
  }

  const char* content_type = mg_get_header(conn, "Content-Type");
  if (content_type == nullptr) {
    processor_.logger_->log_error("Content-Type header missing");
    return false;
  }

  std::string charset;
  const char* charset_begin = strstr(content_type, "charset=");
  if (charset_begin == nullptr) {
    processor_.logger_->log_warn("charset missing from Content-Type header, assuming UTF-8");
    charset = "UTF-8";
  } else {
    charset_begin += strlen("charset=");
    const char* charset_end = strchr(charset_begin, ';');
    if (charset_end == nullptr) {
        charset = std::string(charset_begin);
    } else {
        charset = std::string(charset_begin, charset_end - charset_begin);
    }
  }
  processor_.logger_->log_trace("charset is \"%s\"", charset.c_str());

  std::vector<uint8_t> raw_data;
  {
    std::array<uint8_t, 16384U> buf;
    int read_bytes;
    while ((read_bytes = mg_read(conn, buf.data(), buf.size())) > 0) {
      size_t orig_size = raw_data.size();
      raw_data.resize(orig_size + read_bytes);
      memcpy(raw_data.data() + orig_size, buf.data(), read_bytes);
    }
  }

  if (raw_data.empty()) {
    processor_.logger_->log_error("POST body is empty");
    return false;
  }

  WsXmlDocH doc = ws_xml_read_memory(reinterpret_cast<char*>(raw_data.data()), raw_data.size(), charset.c_str(), 0);

  if (doc == nullptr) {
    processor_.logger_->log_error("Failed to parse POST body as XML");
    return false;
  }

  {
    WsXmlNodeH node = ws_xml_get_doc_root(doc);
    char* xml_buf = nullptr;
    int xml_buf_size = 0;
    ws_xml_dump_memory_node_tree_enc(node, &xml_buf, &xml_buf_size, "UTF-8");
    if (xml_buf != nullptr) {
        logging::LOG_TRACE(processor_.logger_) << "Received request: \"" << std::string(xml_buf, xml_buf_size) << "\"";
        ws_xml_free_memory(xml_buf);
    }
  }

  if (endpoint == processor_.subscription_manager_path_) {
    return this->handleSubscriptionManager(conn, endpoint, doc);
  } else if (strncmp(endpoint, processor_.subscriptions_base_path_.c_str(), processor_.subscriptions_base_path_.length()) == 0) {
    return this->handleSubscriptions(conn, endpoint, doc);
  } else {
    ws_xml_destroy_doc(doc);
    return false;
  }
}

std::string SourceInitiatedSubscriptionListener::Handler::getSoapAction(WsXmlDocH doc) {
  WsXmlNodeH header = ws_xml_get_soap_header(doc);
  if (header == nullptr) {
    return "";
  }
  WsXmlNodeH action_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_ADDRESSING, WSA_ACTION);
  if (action_node == nullptr) {
    return "";
  }
  char* text = ws_xml_get_node_text(action_node);
  if (text == nullptr) {
    return "";
  }

  return text;
}

std::string SourceInitiatedSubscriptionListener::Handler::getMachineId(WsXmlDocH doc) {
  WsXmlNodeH header = ws_xml_get_soap_header(doc);
  if (header == nullptr) {
    return "";
  }
  WsXmlNodeH machineid_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_CUSTOM_MACHINEID, "MachineID");
  if (machineid_node == nullptr) {
    return "";
  }
  char* text = ws_xml_get_node_text(machineid_node);
  if (text == nullptr) {
    return "";
  }

  return text;
}

bool SourceInitiatedSubscriptionListener::Handler::isAckRequested(WsXmlDocH doc) {
  WsXmlNodeH header = ws_xml_get_soap_header(doc);
  if (header == nullptr) {
    return false;
  }
  WsXmlNodeH ack_requested_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_WS_MAN, WSM_ACKREQUESTED);
  return ack_requested_node != nullptr;
}

void SourceInitiatedSubscriptionListener::Handler::sendResponse(struct mg_connection* conn, const std::string& machineId, const std::string& remoteIp, char* xml_buf, size_t xml_buf_size) {
  logging::LOG_TRACE(processor_.logger_) << "Sending response to " << machineId << " (" << remoteIp << "): \"" << std::string(xml_buf, xml_buf_size) << "\"";

  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  mg_printf(conn, "Content-Type: application/soap+xml;charset=UTF-8\r\n");
  mg_printf(conn, "Authorization: %s\r\n", WSMAN_SECURITY_PROFILE_HTTPS_MUTUAL);
  mg_printf(conn, "Content-Length: %zu\r\n", xml_buf_size);
  mg_printf(conn, "\r\n");
  mg_printf(conn, "%.*s", static_cast<int>(xml_buf_size), xml_buf);
}

bool SourceInitiatedSubscriptionListener::Handler::handleSubscriptionManager(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request) {
  const auto request_guard = gsl::finally([&]() {
      ws_xml_destroy_doc(request);
  });

  auto action = getSoapAction(request);
  auto machine_id = getMachineId(request);
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  std::string remote_ip = req_info->remote_addr;
  if (action != ENUM_ACTION_ENUMERATE) {
    processor_.logger_->log_error("%s called by %s (%s) with unknown Action \"%s\"", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str(), action.c_str());
    return false;  // TODO(bakaid): generate fault if possible
  }

  // Create reponse envelope from request
  WsXmlDocH response = wsman_create_response_envelope(request, nullptr);
  const auto response_guard = gsl::finally([&]() {
    ws_xml_destroy_doc(response);
  });

  // Header
  WsXmlNodeH response_header = ws_xml_get_soap_header(response);
  // Header/MessageID
  utils::Identifier msg_id = utils::IdGenerator::getIdGenerator()->generate();
  ws_xml_add_child_format(response_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

  // Body
  WsXmlNodeH response_body = ws_xml_get_soap_body(response);
  // Body/EnumerationResponse
  WsXmlNodeH enumeration_response = ws_xml_add_child(response_body, XML_NS_ENUMERATION, WSENUM_ENUMERATE_RESP, nullptr);
  // Body/EnumerationResponse/EnumerationContext
  ws_xml_add_child(enumeration_response, XML_NS_ENUMERATION, WSENUM_ENUMERATION_CONTEXT, nullptr);
  // Body/EnumerationResponse/Items
  WsXmlNodeH enumeration_items = ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_ITEMS, nullptr);
  // Body/EnumerationResponse/EndOfSequence
  ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_END_OF_SEQUENCE, nullptr);

  // Body/EnumerationResponse/Items/Subscription
  WsXmlNodeH subscription = ws_xml_add_child(enumeration_items, nullptr, "Subscription", nullptr);
  ws_xml_set_ns(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "m");

  // Body/EnumerationResponse/Items/Subscription/Version
  std::lock_guard<std::mutex> lock(processor_.mutex_);
  auto it = processor_.subscribers_.find(machine_id);

  std::string subscription_version;
  if (it != processor_.subscribers_.end() && it->second.subscription_ != nullptr) {
    subscription_version = it->second.subscription_version_;
  } else {
    utils::Identifier id = utils::IdGenerator::getIdGenerator()->generate();
    subscription_version = id.to_string();
  }
  ws_xml_add_child_format(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "Version", "uuid:%s", subscription_version.c_str());

  // Body/EnumerationResponse/Items/Subscription/Envelope
  std::string subscription_identifier;
  std::string subscription_endpoint;
  if (it != processor_.subscribers_.end() && it->second.subscription_ != nullptr) {
    WsXmlNodeH subscription_node = ws_xml_get_doc_root(it->second.subscription_);
    ws_xml_copy_node(subscription_node, subscription);
  } else {
    WsXmlDocH subscription_doc = ws_xml_create_envelope();

    // Header
    WsXmlNodeH header = ws_xml_get_soap_header(subscription_doc);
    WsXmlNodeH node;

    // Header/Action
    node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_ACTION, EVT_ACTION_SUBSCRIBE);
    ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

    // Header/MessageID
    utils::Identifier msg_id = utils::IdGenerator::getIdGenerator()->generate();
    ws_xml_add_child_format(header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

    // Header/To
    node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_TO, WSA_TO_ANONYMOUS);
    ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

    // Header/ResourceURI
    node = ws_xml_add_child(header, XML_NS_WS_MAN, WSM_RESOURCE_URI, "http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog");
    ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

    // Header/ReplyTo
    node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_REPLY_TO, nullptr);
    node = ws_xml_add_child(node, XML_NS_ADDRESSING, WSA_ADDRESS, WSA_TO_ANONYMOUS);
    ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

    // Header/OptionSet
    WsXmlNodeH option_set = ws_xml_add_child(header, XML_NS_WS_MAN, WSM_OPTION_SET, nullptr);
    ws_xml_ns_add(option_set, XML_NS_SCHEMA_INSTANCE, XML_NS_SCHEMA_INSTANCE_PREFIX);

    // Header/OptionSet/Option (CDATA)
    node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, nullptr);
    ws_xml_add_node_attr(node, nullptr, WSM_NAME, "CDATA");
    ws_xml_add_node_attr(node, XML_NS_SCHEMA_INSTANCE, XML_SCHEMA_NIL, "true");

    // Header/OptionSet/Option (IgnoreChannelError)
    node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, nullptr);
    ws_xml_add_node_attr(node, nullptr, WSM_NAME, "IgnoreChannelError");
    ws_xml_add_node_attr(node, XML_NS_SCHEMA_INSTANCE, XML_SCHEMA_NIL, "true");

    // Body
    WsXmlNodeH body = ws_xml_get_soap_body(subscription_doc);
    WsXmlNodeH subscribe_node = ws_xml_add_child(body, XML_NS_EVENTING, WSEVENT_SUBSCRIBE, nullptr);

    // Body/Delivery
    {
      utils::Identifier id = utils::IdGenerator::getIdGenerator()->generate();
      subscription_identifier = id.to_string();
    }
    {
      utils::Identifier id = utils::IdGenerator::getIdGenerator()->generate();
      subscription_endpoint = processor_.subscriptions_base_path_ + "/" + id.to_string();
    }

    WsXmlNodeH delivery_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_DELIVERY, nullptr);
    ws_xml_add_node_attr(delivery_node, nullptr, WSEVENT_DELIVERY_MODE, WSEVENT_DELIVERY_MODE_EVENTS);

    // Body/Delivery/Heartbeats
    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_HEARTBEATS, millisecondsToXsdDuration(processor_.heartbeat_interval_).c_str());

    // Body/Delivery/ConnectionRetry
    auto connection_retry_node = ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_CONNECTIONRETRY, millisecondsToXsdDuration(processor_.connection_retry_interval_).c_str());
    ws_xml_add_node_attr(connection_retry_node, nullptr, "Total", std::to_string(processor_.connection_retry_count_).c_str());

    // Body/Delivery/NotifyTo and Body/EndTo are the same, so we will use this lambda to recreate the same tree
    auto apply_endpoint_nodes = [&](WsXmlNodeH target_node) {
      // ${target_node}/NotifyTo/Address
      ws_xml_add_child_format(target_node, XML_NS_ADDRESSING, WSA_ADDRESS, "https://%s:%hu%s",
                              processor_.listen_hostname_.c_str(),
                              processor_.listen_port_,
                              subscription_endpoint.c_str());
      // ${target_node}/ReferenceProperties
      node = ws_xml_add_child(target_node, XML_NS_ADDRESSING, WSA_REFERENCE_PROPERTIES, nullptr);
      // ${target_node}/ReferenceProperties/Identifier
      ws_xml_add_child_format(node, XML_NS_EVENTING, WSEVENT_IDENTIFIER, "%s", subscription_identifier.c_str());
      // ${target_node}/Policy
      WsXmlNodeH policy = ws_xml_add_child(target_node, nullptr, "Policy", nullptr);
      ws_xml_set_ns(policy, XML_NS_CUSTOM_POLICY, "c");
      ws_xml_ns_add(policy, XML_NS_CUSTOM_AUTHENTICATION, "auth");
      // ${target_node}/Policy/ExactlyOne
      WsXmlNodeH exactly_one = ws_xml_add_child(policy, XML_NS_CUSTOM_POLICY, "ExactlyOne", nullptr);
      // ${target_node}/Policy/ExactlyOne/All
      WsXmlNodeH all = ws_xml_add_child(exactly_one, XML_NS_CUSTOM_POLICY, "All", nullptr);
      // ${target_node}/Policy/ExactlyOne/All/Authentication
      WsXmlNodeH authentication = ws_xml_add_child(all, XML_NS_CUSTOM_AUTHENTICATION, "Authentication", nullptr);
      ws_xml_add_node_attr(authentication, nullptr, "Profile", WSMAN_SECURITY_PROFILE_HTTPS_MUTUAL);
      // ${target_node}/Policy/ExactlyOne/All/Authentication/ClientCertificate
      WsXmlNodeH client_certificate = ws_xml_add_child(authentication, XML_NS_CUSTOM_AUTHENTICATION, "ClientCertificate", nullptr);
      // ${target_node}/Policy/ExactlyOne/All/Authentication/ClientCertificate/Thumbprint
      WsXmlNodeH thumbprint = ws_xml_add_child_format(client_certificate, XML_NS_CUSTOM_AUTHENTICATION, "Thumbprint", "%s", processor_.ssl_ca_cert_thumbprint_.c_str());
      ws_xml_add_node_attr(thumbprint, nullptr, "Role", "issuer");
    };

    // Body/Delivery/NotifyTo
    WsXmlNodeH notifyto_node = ws_xml_add_child(delivery_node, XML_NS_EVENTING, WSEVENT_NOTIFY_TO, nullptr);
    apply_endpoint_nodes(notifyto_node);

    // Body/EndTo
    WsXmlNodeH endto_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_ENDTO, nullptr);
    apply_endpoint_nodes(endto_node);

    // Body/MaxElements
    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_MAX_ELEMENTS, std::to_string(processor_.max_elements_).c_str());
    // Body/MaxTime
    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSENUM_MAX_TIME, millisecondsToXsdDuration(processor_.max_latency_).c_str());

    // Body/Expires
    ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_EXPIRES, millisecondsToXsdDuration(processor_.subscription_expiration_interval_).c_str());

    // Body/Filter
    ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_FILTER, processor_.xpath_xml_query_.c_str());
    // ws_xml_add_node_attr(filter_node, nullptr, "Dialect", "http://schemas.microsoft.com/win/2004/08/events/eventquery");

    // Body/Bookmark
    if (it != processor_.subscribers_.end() && it->second.bookmark_ != nullptr) {
      WsXmlNodeH bookmark_node = ws_xml_get_doc_root(it->second.bookmark_);
      ws_xml_copy_node(bookmark_node, subscribe_node);
    } else if (processor_.initial_existing_events_strategy_ == INITIAL_EXISTING_EVENTS_STRATEGY_ALL) {
      ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_BOOKMARK, "http://schemas.dmtf.org/wbem/wsman/1/wsman/bookmark/earliest");
    }

    // Body/SendBookmarks
    ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_SENDBOOKMARKS, nullptr);

    // Copy the whole Subscription
    WsXmlNodeH subscription_node = ws_xml_get_doc_root(subscription_doc);
    ws_xml_copy_node(subscription_node, subscription);

    // Save subscription
    if (it == processor_.subscribers_.end()) {
      it = processor_.subscribers_.emplace(machine_id, SubscriberData()).first;
    }
    it->second.setSubscription(subscription_version, subscription_doc, subscription_endpoint, subscription_identifier);
  }

  // Send response
  char* xml_buf = nullptr;
  int xml_buf_size = 0;
  ws_xml_dump_memory_enc(response, &xml_buf, &xml_buf_size, "UTF-8");

  sendResponse(conn, machine_id, req_info->remote_addr, xml_buf, xml_buf_size);

  ws_xml_free_memory(xml_buf);

  return true;
}

SourceInitiatedSubscriptionListener::Handler::WriteCallback::WriteCallback(char* text)
    : text_(text) {
}

int64_t SourceInitiatedSubscriptionListener::Handler::WriteCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
  const auto write_ret = stream->write(reinterpret_cast<uint8_t*>(text_), strlen(text_));
  return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
}

int SourceInitiatedSubscriptionListener::Handler::enumerateEventCallback(WsXmlNodeH node, void* data) {
  if (data == nullptr) {
    return 1;
  }

  std::shared_ptr<core::ProcessSession> session;
  std::shared_ptr<logging::Logger> logger;
  std::string machine_id;
  std::string remote_ip;
  std::tie(session, logger, machine_id, remote_ip) = *static_cast<std::tuple<std::shared_ptr<core::ProcessSession>, std::shared_ptr<logging::Logger>, std::string, std::string>*>(data);

  char* text = ws_xml_get_node_text(node);
  if (text == nullptr) {
      logger->log_error("Failed to get text for node");
      return 1;
  }

  try {
    logger->log_trace("Found Event");
    auto flow_file = session->create();
    if (flow_file == nullptr) {
      logger->log_error("Failed to create FlowFile");
      return 1;
    }

    WriteCallback callback(text);
    session->write(flow_file, &callback);

    session->putAttribute(flow_file, core::SpecialFlowAttribute::MIME_TYPE, "application/xml");
    flow_file->addAttribute(ATTRIBUTE_WEF_REMOTE_MACHINEID, machine_id);
    flow_file->addAttribute(ATTRIBUTE_WEF_REMOTE_IP, remote_ip);

    session->transfer(flow_file, SourceInitiatedSubscriptionListener::Success);
    session->commit();
  } catch (const std::exception& e) {
    logger->log_error("Caught exception while processing Events: %s", e.what());
    return 1;
  } catch (...) {
    logger->log_error("Caught exception while processing Events");
    return 1;
  }

  return 0;
}

bool SourceInitiatedSubscriptionListener::Handler::handleSubscriptions(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request) {
  const auto guard = gsl::finally([&]() {
      ws_xml_destroy_doc(request);
  });
  auto action = getSoapAction(request);
  auto machine_id = getMachineId(request);
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  std::string remote_ip = req_info->remote_addr;
  if (action == EVT_ACTION_SUBEND) {
    std::lock_guard<std::mutex> lock(processor_.mutex_);
    auto it = processor_.subscribers_.find(machine_id);
    if (it != processor_.subscribers_.end()) {
        processor_.subscribers_.erase(it);
    }
    // TODO(bakaid): make sure whether we really have to clean the bookmark as well (based on the fault)
  } else if (action == WSMAN_CUSTOM_ACTION_HEARTBEAT) {
    processor_.logger_->log_debug("Received Heartbeat on %s from %s (%s)", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
  } else if (action == WSMAN_CUSTOM_ACTION_EVENTS) {
    processor_.logger_->log_debug("Received Events on %s from %s (%s)", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
    // Body
    WsXmlNodeH body = ws_xml_get_soap_body(request);
    if (body == nullptr) {
      processor_.logger_->log_error("Received malformed Events request on %s from %s (%s), SOAP Body missing", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      return false;
    }
    // Body/Events
    WsXmlNodeH events_node = ws_xml_get_child(body, 0 /*index*/, XML_NS_WS_MAN, WSM_EVENTS);
    if (events_node == nullptr) {
      processor_.logger_->log_error("Received malformed Events request on %s from %s (%s), Events missing", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      return false;
    }
    // Enumare Body/Events/Event nodes
    auto session = processor_.session_factory_->createSession();
    std::tuple<std::shared_ptr<core::ProcessSession>, std::shared_ptr<logging::Logger>, std::string, std::string> callback_args =
        std::forward_as_tuple(session, processor_.logger_, machine_id, remote_ip);
    int ret = ws_xml_enum_children(events_node, &SourceInitiatedSubscriptionListener::Handler::enumerateEventCallback, &callback_args, 0 /*bRecursive*/);
    if (ret != 0) {
      processor_.logger_->log_error("Failed to parse events on %s from %s (%s), rolling back session", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      session->rollback();
    }
    // Header
    WsXmlNodeH header = ws_xml_get_soap_header(request);
    // Header/Bookmark
    WsXmlNodeH bookmark_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_WS_MAN, WSM_BOOKMARK);
    if (ret == 0 && bookmark_node != nullptr) {
      WsXmlDocH bookmark_doc = ws_xml_create_doc(XML_NS_WS_MAN, WSM_BOOKMARK);
      WsXmlNodeH temp = ws_xml_get_doc_root(bookmark_doc);
      ws_xml_duplicate_children(temp, bookmark_node);

      std::lock_guard<std::mutex> lock(processor_.mutex_);
      auto it = processor_.subscribers_.find(machine_id);
      if (it != processor_.subscribers_.end()) {
        it = processor_.subscribers_.emplace(machine_id, SubscriberData()).first;
      }
      it->second.setBookmark(bookmark_doc);
      // Bookmark changed, invalidate stored subscription
      it->second.clearSubscription();

      // Persist state
      processor_.persistState();

      char* xml_buf = nullptr;
      int xml_buf_size = 0;
      ws_xml_dump_memory_enc(bookmark_doc, &xml_buf, &xml_buf_size, "UTF-8");
      processor_.logger_->log_debug("Saved new bookmark for %s: \"%.*s\"", machine_id.c_str(), xml_buf_size, xml_buf);
      ws_xml_free_memory(xml_buf);
    }
  } else {
    processor_.logger_->log_error("%s called by %s (%s) with unknown Action \"%s\"", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str(), action.c_str());
    return false;  // TODO(bakaid): generate fault if possible
  }

  if (isAckRequested(request)) {
    // Assemble ACK
    WsXmlDocH ack = wsman_create_response_envelope(request, WSMAN_CUSTOM_ACTION_ACK);
    // Header
    WsXmlNodeH ack_header = ws_xml_get_soap_header(ack);

    // Header/MessageID
    utils::Identifier msg_id = utils::IdGenerator::getIdGenerator()->generate();
    ws_xml_add_child_format(ack_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

    // Send ACK
    char* xml_buf = nullptr;
    int xml_buf_size = 0;
    ws_xml_dump_memory_enc(ack, &xml_buf, &xml_buf_size, "UTF-8");

    sendResponse(conn, machine_id, remote_ip, xml_buf, xml_buf_size);

    ws_xml_free_memory(xml_buf);
    ws_xml_destroy_doc(ack);
  }

  return true;
}

void SourceInitiatedSubscriptionListener::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& /*session*/) {
  logger_->log_trace("SourceInitiatedSubscriptionListener onTrigger called");
}

void SourceInitiatedSubscriptionListener::initialize() {
  logger_->log_trace("Initializing SourceInitiatedSubscriptionListener");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(ListenHostname);
  properties.insert(ListenPort);
  properties.insert(SubscriptionManagerPath);
  properties.insert(SubscriptionsBasePath);
  properties.insert(SSLCertificate);
  properties.insert(SSLCertificateAuthority);
  properties.insert(SSLVerifyPeer);
  properties.insert(XPathXmlQuery);
  properties.insert(InitialExistingEventsStrategy);
  properties.insert(SubscriptionExpirationInterval);
  properties.insert(HeartbeatInterval);
  properties.insert(MaxElements);
  properties.insert(MaxLatency);
  properties.insert(ConnectionRetryInterval);
  properties.insert(ConnectionRetryCount);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void SourceInitiatedSubscriptionListener::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string ssl_certificate_file;
  std::string ssl_ca_file;

  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  std::string value;
  context->getProperty(ListenHostname.getName(), listen_hostname_);
  if (!context->getProperty(ListenPort.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Listen Port attribute is missing or invalid");
  } else {
    core::Property::StringToInt(value, listen_port_);
  }
  context->getProperty(SubscriptionManagerPath.getName(), subscription_manager_path_);
  context->getProperty(SubscriptionsBasePath.getName(), subscriptions_base_path_);
  if (!context->getProperty(SSLCertificate.getName(), ssl_certificate_file)) {
    throw Exception(PROCESSOR_EXCEPTION, "SSL Certificate attribute is missing");
  }
  if (!context->getProperty(SSLCertificateAuthority.getName(), ssl_ca_file)) {
    throw Exception(PROCESSOR_EXCEPTION, "SSL Certificate Authority attribute is missing");
  }
  if (!context->getProperty(SSLVerifyPeer.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "SSL Verify Peer attribute is missing");
  }
  bool verify_peer = utils::StringUtils::toBool(value).value_or(true);
  context->getProperty(XPathXmlQuery.getName(), xpath_xml_query_);
  if (!context->getProperty(InitialExistingEventsStrategy.getName(), initial_existing_events_strategy_)) {
    throw Exception(PROCESSOR_EXCEPTION, "Initial Existing Events Strategy attribute is missing or invalid");
  }
  if (!context->getProperty(SubscriptionExpirationInterval.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Subscription Expiration Interval attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, subscription_expiration_interval_, unit) ||
        !core::Property::ConvertTimeUnitToMS(subscription_expiration_interval_, unit, subscription_expiration_interval_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Subscription Expiration Interval attribute is invalid");
    }
  }
  if (!context->getProperty(HeartbeatInterval.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Heartbeat Interval attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, heartbeat_interval_, unit) || !core::Property::ConvertTimeUnitToMS(heartbeat_interval_, unit, heartbeat_interval_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Heartbeat Interval attribute is invalid");
    }
  }
  if (!context->getProperty(MaxElements.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Max Elements attribute is missing or invalid");
  } else if (!core::Property::StringToInt(value, max_elements_)) {
    throw Exception(PROCESSOR_EXCEPTION, "Max Elements attribute is invalid");
  }
  if (!context->getProperty(MaxLatency.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Max Latency attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, max_latency_, unit) || !core::Property::ConvertTimeUnitToMS(max_latency_, unit, max_latency_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Max Latency attribute is invalid");
    }
  }
  if (!context->getProperty(ConnectionRetryInterval.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Connection Retry Interval attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, connection_retry_interval_, unit) || !core::Property::ConvertTimeUnitToMS(connection_retry_interval_, unit, connection_retry_interval_)) {
      throw Exception(PROCESSOR_EXCEPTION, "Connection Retry Interval attribute is invalid");
    }
  }
  if (!context->getProperty(ConnectionRetryCount.getName(), value)) {
    throw Exception(PROCESSOR_EXCEPTION, "Connection Retry Count attribute is missing or invalid");
  } else if (!core::Property::StringToInt(value, connection_retry_count_)) {
    throw Exception(PROCESSOR_EXCEPTION, "Connection Retry Count attribute is invalid");
  }

  FILE* fp = fopen(ssl_ca_file.c_str(), "rb");
  if (fp == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to open file specified by SSL Certificate Authority attribute");
  }
  X509* ca = nullptr;
  PEM_read_X509(fp, &ca, nullptr, nullptr);
  fclose(fp);
  if (ca == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to parse file specified by SSL Certificate Authority attribute");
  }
  utils::tls::X509_unique_ptr ca_ptr{ca};

  std::array<uint8_t, 20U> hash_buf;
  int ret = X509_digest(ca, EVP_sha1(), hash_buf.data(), nullptr);
  if (ret != 1) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get fingerprint for CA specified by SSL Certificate Authority attribute");
  }
  ssl_ca_cert_thumbprint_ = utils::StringUtils::to_hex(hash_buf.data(), hash_buf.size(), true /*uppercase*/);
  logger_->log_debug("%s SHA-1 thumbprint is %s", ssl_ca_file.c_str(), ssl_ca_cert_thumbprint_.c_str());

  session_factory_ = sessionFactory;

  // Load state
  loadState();

  // Start server
  std::vector<std::string> options;
  options.emplace_back("enable_keep_alive");
  options.emplace_back("yes");
  options.emplace_back("keep_alive_timeout_ms");
  options.emplace_back("15000");
  options.emplace_back("num_threads");
  options.emplace_back("1");
  options.emplace_back("listening_ports");
  options.emplace_back(std::to_string(listen_port_) + "s");
  options.emplace_back("ssl_certificate");
  options.emplace_back(ssl_certificate_file);
  options.emplace_back("ssl_ca_file");
  options.emplace_back(ssl_ca_file);
  options.emplace_back("ssl_verify_peer");
  options.emplace_back(verify_peer ? "yes" : "no");

  try {
    server_ = std::unique_ptr<CivetServer>(new CivetServer(options));
  } catch (const std::exception& e) {
    throw Exception(PROCESSOR_EXCEPTION, std::string("Failed to initialize server, error: ") + e.what());
  } catch (...) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to initialize server");
  }
  handler_ = std::unique_ptr<Handler>(new Handler(*this));
  server_->addHandler("**", *handler_);
}

void SourceInitiatedSubscriptionListener::notifyStop() {
  server_.release();
}

REGISTER_RESOURCE(SourceInitiatedSubscriptionListener, "This processor implements a Windows Event Forwarding Source Initiated Subscription server with the help of OpenWSMAN. "
                                                       "Windows hosts can be set up to connect and forward Event Logs to this processor.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

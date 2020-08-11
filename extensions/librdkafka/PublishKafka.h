/**
 * @file PublishKafka.h
 * PublishKafka class declaration
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
#ifndef EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_
#define EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <condition_variable>
#include <utility>
#include <vector>

#include "utils/GeneralUtils.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/logging/Logger.h"
#include "utils/RegexUtils.h"
#include "rdkafka.h"
#include "KafkaConnection.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PublishKafka Class
class PublishKafka : public core::Processor {
 public:
  static constexpr char const* ProcessorName = "PublishKafka";

  // Supported Properties
  static const core::Property SeedBrokers;
  static const core::Property Topic;
  static const core::Property DeliveryGuarantee;
  static const core::Property MaxMessageSize;
  static const core::Property RequestTimeOut;
  static const core::Property MessageTimeOut;
  static const core::Property ClientName;
  static const core::Property BatchSize;
  static const core::Property TargetBatchPayloadSize;
  static const core::Property AttributeNameRegex;
  static const core::Property QueueBufferMaxTime;
  static const core::Property QueueBufferMaxSize;
  static const core::Property QueueBufferMaxMessage;
  static const core::Property CompressCodec;
  static const core::Property MaxFlowSegSize;
  static const core::Property SecurityProtocol;
  static const core::Property SecurityCA;
  static const core::Property SecurityCert;
  static const core::Property SecurityPrivateKey;
  static const core::Property SecurityPrivateKeyPassWord;
  static const core::Property KerberosServiceName;
  static const core::Property KerberosPrincipal;
  static const core::Property KerberosKeytabPath;
  static const core::Property MessageKeyField;
  static const core::Property DebugContexts;
  static const core::Property FailEmptyFlowFiles;

  // Supported Relationships
  static const core::Relationship Failure;
  static const core::Relationship Success;

  explicit PublishKafka(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::Processor(std::move(name), uuid) {
  }

  ~PublishKafka() override = default;

  bool supportsDynamicProperties() override { return true; }

  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void notifyStop() override;

  class Messages;

 protected:
  bool configureNewConnection(const std::shared_ptr<core::ProcessContext> &context);
  bool createNewTopic(const std::shared_ptr<core::ProcessContext> &context, const std::string& topic_name);

 private:
  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<PublishKafka>::getLogger()};

  KafkaConnectionKey key_;
  std::unique_ptr<KafkaConnection> conn_;
  std::mutex connection_mutex_;

  uint32_t batch_size_{};
  uint64_t target_batch_payload_size_{};
  uint64_t max_flow_seg_size_{};
  utils::Regex attributeNameRegex_;

  std::atomic<bool> interrupted_{false};
  std::mutex messages_mutex_;
  std::set<std::shared_ptr<Messages>> messages_set_;
};

REGISTER_RESOURCE(PublishKafka, "This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. "
                  "This message is optionally assigned a key by using the <Kafka Key> Property.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_LIBRDKAFKA_PUBLISHKAFKA_H_

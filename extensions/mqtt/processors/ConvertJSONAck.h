/**
 * @file ConvertAck.h
 * ConvertAck class declaration
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
#ifndef __CONVERT_ACKNOWLEDGEMENT_H__
#define __CONVERT_ACKNOWLEDGEMENT_H__

#include "MQTTControllerService.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTClient.h"
#include "c2/protocols/RESTProtocol.h"
#include "ConvertBase.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

/**
 * Purpose: Converts JSON acks into an MQTT consumable by
 * MQTTC2Protocol.
 */
class ConvertJSONAck : public ConvertBase {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit ConvertJSONAck(std::string name, utils::Identifier uuid = utils::Identifier())
      : ConvertBase(name, uuid),
        logger_(logging::LoggerFactory<ConvertJSONAck>::getLogger()) {
  }
  // Destructor
  virtual ~ConvertJSONAck() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "ConvertJSONAck";


 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 protected:

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback() {
    }
    ~ReadCallback() {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t ret = 0;
      if (nullptr == stream)
        return 0;
      buffer_.resize(stream->getSize());
      ret = stream->read(reinterpret_cast<uint8_t*>(buffer_.data()), stream->getSize());
      return ret;
    }
    std::vector<char> buffer_;
  };

  /**
   * Parse Topic name from the json -- given a known structure that we expect.
   * @param json json representation defined by the restful protocol
   */
  std::string parseTopicName(const std::string &json);
 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

/**
 * @file HashContent.h
 * HashContent class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_HASHCONTENT_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_HASHCONTENT_H_

#ifdef OPENSSL_SUPPORT

#include <openssl/md5.h>
#include <openssl/sha.h>

#include <stdint.h>

#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "io/BaseStream.h"
#include "utils/StringUtils.h"

using HashReturnType = std::pair<std::string, int64_t>;

// Without puttng this into its own namespace, the code would export already defined symbols.
namespace { // NOLINT
#define HASH_BUFFER_SIZE 16384

  HashReturnType MD5Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    uint8_t buffer[HASH_BUFFER_SIZE];
    MD5_CTX context;
    MD5_Init(&context);

    size_t ret = 0;
    do {
      ret = stream->readData(buffer, HASH_BUFFER_SIZE);
      if (ret > 0) {
        MD5_Update(&context, buffer, ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      unsigned char digest[MD5_DIGEST_LENGTH];
      MD5_Final(digest, &context);
      ret_val.first = utils::StringUtils::to_hex(digest, MD5_DIGEST_LENGTH, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA1Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    uint8_t buffer[HASH_BUFFER_SIZE];
    SHA_CTX context;
    SHA1_Init(&context);

    size_t ret = 0;
    do {
      ret = stream->readData(buffer, HASH_BUFFER_SIZE);
      if (ret > 0) {
        SHA1_Update(&context, buffer, ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      unsigned char digest[SHA_DIGEST_LENGTH];
      SHA1_Final(digest, &context);
      ret_val.first = utils::StringUtils::to_hex(digest, SHA_DIGEST_LENGTH, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA256Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    uint8_t buffer[HASH_BUFFER_SIZE];
    SHA256_CTX context;
    SHA256_Init(&context);

    size_t ret;
    do {
      ret = stream->readData(buffer, HASH_BUFFER_SIZE);
      if (ret > 0) {
        SHA256_Update(&context, buffer, ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      unsigned char digest[SHA256_DIGEST_LENGTH];
      SHA256_Final(digest, &context);
      ret_val.first = utils::StringUtils::to_hex(digest, SHA256_DIGEST_LENGTH, true /*uppercase*/);
    }
    return ret_val;
  }
}  // namespace


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

static const std::map<std::string, const std::function<HashReturnType(const std::shared_ptr<io::BaseStream>&)>> HashAlgos =
  { {"MD5",  MD5Hash}, {"SHA1", SHA1Hash}, {"SHA256", SHA256Hash} };

//! HashContent Class
class HashContent : public core::Processor {
 public:
  //! Constructor
  /*!
  * Create a new processor
  */
  explicit HashContent(std::string name,  utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid) {
    logger_ = logging::LoggerFactory<HashContent>::getLogger();
  }
  //! Processor Name
  static constexpr char const* ProcessorName = "HashContent";
  //! Supported Properties
  static core::Property HashAttribute;
  static core::Property HashAlgorithm;
  static core::Property FailOnEmpty;
  //! Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);  // override

  //! OnTrigger method, implemented by NiFi HashContent
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session);  // override
  //! Initialize, over write by NiFi HashContent
  void initialize(void);  // override

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(std::shared_ptr<core::FlowFile> flowFile, const HashContent& parent);
    ~ReadCallback() {}
    int64_t process(std::shared_ptr<io::BaseStream> stream);

   private:
    std::shared_ptr<core::FlowFile> flowFile_;
    const HashContent& parent_;
  };

 private:
  //! Logger
  std::shared_ptr<logging::Logger> logger_;
  std::string algoName_;
  std::string attrKey_;
  bool failOnEmpty_;
};

REGISTER_RESOURCE(HashContent,"HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. Configuration options exist to select hashing algorithm and set the name of the attribute."); // NOLINT

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // OPENSSL_SUPPORT

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_HASHCONTENT_H_

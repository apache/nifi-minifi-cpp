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

#include <array>
#include <cstdint>
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "io/BaseStream.h"
#include "utils/StringUtils.h"
#include "utils/Export.h"

using HashReturnType = std::pair<std::string, int64_t>;

// Without puttng this into its own namespace, the code would export already defined symbols.
namespace { // NOLINT
#define HASH_BUFFER_SIZE 16384

  HashReturnType MD5Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    std::array<std::byte, HASH_BUFFER_SIZE> buffer{};
    MD5_CTX context;
    MD5_Init(&context);

    size_t ret = 0;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        MD5_Update(&context, buffer.data(), ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, MD5_DIGEST_LENGTH> digest{};
      MD5_Final(reinterpret_cast<unsigned char*>(digest.data()), &context);
      ret_val.first = org::apache::nifi::minifi::utils::StringUtils::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA1Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    std::array<std::byte, HASH_BUFFER_SIZE> buffer{};
    SHA_CTX context;
    SHA1_Init(&context);

    size_t ret = 0;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        SHA1_Update(&context, buffer.data(), ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, SHA_DIGEST_LENGTH> digest{};
      SHA1_Final(reinterpret_cast<unsigned char*>(digest.data()), &context);
      ret_val.first = org::apache::nifi::minifi::utils::StringUtils::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA256Hash(const std::shared_ptr<org::apache::nifi::minifi::io::BaseStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    std::array<std::byte, HASH_BUFFER_SIZE> buffer{};
    SHA256_CTX context;
    SHA256_Init(&context);

    size_t ret;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        SHA256_Update(&context, buffer.data(), ret);
        ret_val.second += ret;
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, SHA256_DIGEST_LENGTH> digest{};
      SHA256_Final(reinterpret_cast<unsigned char*>(digest.data()), &context);
      ret_val.first = org::apache::nifi::minifi::utils::StringUtils::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }
}  // namespace


namespace org::apache::nifi::minifi::processors {

static const std::map<std::string, const std::function<HashReturnType(const std::shared_ptr<io::BaseStream>&)>> HashAlgos =
  { {"MD5",  MD5Hash}, {"SHA1", SHA1Hash}, {"SHA256", SHA256Hash} };

//! HashContent Class
class HashContent : public core::Processor {
 public:
  //! Constructor
  /*!
  * Create a new processor
  */
  explicit HashContent(const std::string& name,  const utils::Identifier& uuid = {})
      : Processor(name, uuid) {
  }
  //! Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "HashContent";
  //! Supported Properties
  EXTENSIONAPI static core::Property HashAttribute;
  EXTENSIONAPI static core::Property HashAlgorithm;
  EXTENSIONAPI static core::Property FailOnEmpty;
  //! Supported Relationships
  EXTENSIONAPI static core::Relationship Success;
  EXTENSIONAPI static core::Relationship Failure;

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;

  //! OnTrigger method, implemented by NiFi HashContent
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  //! Initialize, over write by NiFi HashContent
  void initialize() override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  //! Logger
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HashContent>::getLogger();
  std::string algoName_;
  std::string attrKey_;
  bool failOnEmpty_{};
};

}  // namespace org::apache::nifi::minifi::processors

#endif  // OPENSSL_SUPPORT

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_HASHCONTENT_H_

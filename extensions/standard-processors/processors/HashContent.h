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
#pragma once

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
#include "utils/StringUtils.h"
#include "utils/Export.h"

using HashReturnType = std::pair<std::string, int64_t>;

// Without puttng this into its own namespace, the code would export already defined symbols.
namespace { // NOLINT
#define HASH_BUFFER_SIZE 16384

  HashReturnType MD5Hash(const std::shared_ptr<org::apache::nifi::minifi::io::InputStream>& stream) {
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

  HashReturnType SHA1Hash(const std::shared_ptr<org::apache::nifi::minifi::io::InputStream>& stream) {
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

  HashReturnType SHA256Hash(const std::shared_ptr<org::apache::nifi::minifi::io::InputStream>& stream) {
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

static const std::map<std::string, const std::function<HashReturnType(const std::shared_ptr<io::InputStream>&)>> HashAlgos =
  { {"MD5",  MD5Hash}, {"SHA1", SHA1Hash}, {"SHA256", SHA256Hash} };

class HashContent : public core::Processor {
 public:
  explicit HashContent(std::string name,  const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. "
      "Configuration options exist to select hashing algorithm and set the name of the attribute.";

  EXTENSIONAPI static const core::Property HashAttribute;
  EXTENSIONAPI static const core::Property HashAlgorithm;
  EXTENSIONAPI static const core::Property FailOnEmpty;
  static auto properties() {
    return std::array{
      HashAttribute,
      HashAlgorithm,
      FailOnEmpty
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HashContent>::getLogger();
  std::string algoName_;
  std::string attrKey_;
  bool failOnEmpty_{};
};

}  // namespace org::apache::nifi::minifi::processors

#endif  // OPENSSL_SUPPORT

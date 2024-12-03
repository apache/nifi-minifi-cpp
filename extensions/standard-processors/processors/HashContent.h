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

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <sstream>
#include <utility>

#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/RelationshipDefinition.h"
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
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const auto guard = gsl::finally([&context]() {
      EVP_MD_CTX_free(context);
    });
    EVP_DigestInit_ex(context, EVP_md5(), nullptr);

    size_t ret = 0;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        EVP_DigestUpdate(context, buffer.data(), ret);
        ret_val.second += gsl::narrow<int64_t>(ret);
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, MD5_DIGEST_LENGTH> digest{};
      EVP_DigestFinal_ex(context, reinterpret_cast<unsigned char*>(digest.data()), nullptr);
      ret_val.first = org::apache::nifi::minifi::utils::string::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA1Hash(const std::shared_ptr<org::apache::nifi::minifi::io::InputStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    std::array<std::byte, HASH_BUFFER_SIZE> buffer{};
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const auto guard = gsl::finally([&context]() {
      EVP_MD_CTX_free(context);
    });
    EVP_DigestInit_ex(context, EVP_sha1(), nullptr);

    size_t ret = 0;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        EVP_DigestUpdate(context, buffer.data(), ret);
        ret_val.second += gsl::narrow<int64_t>(ret);
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, SHA_DIGEST_LENGTH> digest{};
      EVP_DigestFinal_ex(context, reinterpret_cast<unsigned char*>(digest.data()), nullptr);
      ret_val.first = org::apache::nifi::minifi::utils::string::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }

  HashReturnType SHA256Hash(const std::shared_ptr<org::apache::nifi::minifi::io::InputStream>& stream) {
    HashReturnType ret_val;
    ret_val.second = 0;
    std::array<std::byte, HASH_BUFFER_SIZE> buffer{};
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    const auto guard = gsl::finally([&context]() {
      EVP_MD_CTX_free(context);
    });
    EVP_DigestInit_ex(context, EVP_sha256(), nullptr);

    size_t ret;
    do {
      ret = stream->read(buffer);
      if (ret > 0) {
        EVP_DigestUpdate(context, buffer.data(), ret);
        ret_val.second += gsl::narrow<int64_t>(ret);
      }
    } while (ret > 0);

    if (ret_val.second > 0) {
      std::array<std::byte, SHA256_DIGEST_LENGTH> digest{};
      EVP_DigestFinal_ex(context, reinterpret_cast<unsigned char*>(digest.data()), nullptr);
      ret_val.first = org::apache::nifi::minifi::utils::string::to_hex(digest, true /*uppercase*/);
    }
    return ret_val;
  }
}  // namespace


namespace org::apache::nifi::minifi::processors {

static const std::map<std::string, const std::function<HashReturnType(const std::shared_ptr<io::InputStream>&)>> HashAlgos =
  { {"MD5",  MD5Hash}, {"SHA1", SHA1Hash}, {"SHA256", SHA256Hash} };

class HashContent : public core::ProcessorImpl {
 public:
  explicit HashContent(std::string_view name,  const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. "
      "Configuration options exist to select hashing algorithm and set the name of the attribute.";

  EXTENSIONAPI static constexpr auto HashAttribute = core::PropertyDefinitionBuilder<>::createProperty("Hash Attribute")
      .withDescription("Attribute to store checksum to")
      .withDefaultValue("Checksum")
      .build();
  EXTENSIONAPI static constexpr auto HashAlgorithm = core::PropertyDefinitionBuilder<>::createProperty("Hash Algorithm")
      .withDescription("Name of the algorithm used to generate checksum")
      .withDefaultValue("SHA256")
      .build();
  EXTENSIONAPI static constexpr auto FailOnEmpty = core::PropertyDefinitionBuilder<>::createProperty("Fail on empty")
      .withDescription("Route to failure relationship in case of empty content")
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      HashAttribute,
      HashAlgorithm,
      FailOnEmpty
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "success operational on the flow record"};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "failure operational on the flow record"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<HashContent>::getLogger(uuid_);
  std::function<HashReturnType(const std::shared_ptr<io::InputStream>&)> algorithm_ = SHA256Hash;
  std::string attrKey_;
  bool failOnEmpty_{};
};

}  // namespace org::apache::nifi::minifi::processors

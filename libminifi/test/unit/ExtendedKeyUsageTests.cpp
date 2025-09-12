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
#include <openssl/x509v3.h>

#include "minifi-cpp/utils/gsl.h"
#include "utils/tls/ExtendedKeyUsage.h"
#include "unit/Catch.h"

namespace utils = org::apache::nifi::minifi::utils;

namespace {

constexpr unsigned char SEQUENCE_TAG = 0x30;
constexpr unsigned char OID_TAG = 6;
constexpr unsigned char OID_LENGTH = 8;
constexpr unsigned char OID_ENCODED_LENGTH = 2 + OID_LENGTH;
constexpr std::array<unsigned char, OID_LENGTH - 1> OID_PREFIX{0x2b, 6, 1, 5, 5, 7, 3};

// {2, 1} -> {30 14 6 8 2b 6 1 5 5 7 3 2 6 8 2b 6 1 5 5 7 3 1}
//            ^---^ sequence tag + length
//                  ^-^                  ^-^ OID tag + length
//                      ^-client auth--^     ^-server auth--^
std::vector<unsigned char> createDerEncodedExtendedKeyUsage(const std::vector<uint8_t>& last_bytes_of_oids) {
  size_t oids_size = OID_ENCODED_LENGTH * last_bytes_of_oids.size();
  std::vector<unsigned char> der_encoded_key_usage(2 + oids_size);

  der_encoded_key_usage[0] = SEQUENCE_TAG;
  der_encoded_key_usage[1] = gsl::narrow<unsigned char>(oids_size);

  for (size_t i = 0; i < last_bytes_of_oids.size(); ++i) {
    der_encoded_key_usage[2 + (OID_ENCODED_LENGTH * i)] = OID_TAG;
    der_encoded_key_usage[3 + (OID_ENCODED_LENGTH * i)] = OID_LENGTH;
    for (size_t j = 0; j < OID_PREFIX.size(); ++j) {
      der_encoded_key_usage[4 + j + (OID_ENCODED_LENGTH * i)] = OID_PREFIX[j];
    }
    der_encoded_key_usage[4 + OID_PREFIX.size() + (OID_ENCODED_LENGTH * i)] = last_bytes_of_oids[i];
  }
  return der_encoded_key_usage;
}

utils::tls::EXTENDED_KEY_USAGE_unique_ptr createExtendedKeyUsage(const std::vector<uint8_t>& last_bytes_of_oids) {
  std::vector<unsigned char> der_encoded_key_usage = createDerEncodedExtendedKeyUsage(last_bytes_of_oids);
  const unsigned char* data = der_encoded_key_usage.data();
  long length = gsl::narrow<long>(der_encoded_key_usage.size());  // NOLINT: cpplint hates `long`, but that is the param type in the API
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage{d2i_EXTENDED_KEY_USAGE(nullptr, &data, length)};
  return key_usage;
}

void testIsSubsetOf(
    const utils::tls::ExtendedKeyUsage& key_usage_empty,
    const utils::tls::ExtendedKeyUsage& key_usage_clientauth,
    const utils::tls::ExtendedKeyUsage& key_usage_clientauth_serverauth,
    const utils::tls::ExtendedKeyUsage& key_usage_clientauth_serverauth_codesigning,
    const utils::tls::ExtendedKeyUsage& key_usage_clientauth_serverauth_timestamping) {
  REQUIRE(key_usage_empty.isSubsetOf(key_usage_empty));
  REQUIRE(key_usage_empty.isSubsetOf(key_usage_clientauth));
  REQUIRE(key_usage_empty.isSubsetOf(key_usage_clientauth_serverauth));
  REQUIRE(key_usage_empty.isSubsetOf(key_usage_clientauth_serverauth_codesigning));
  REQUIRE(key_usage_empty.isSubsetOf(key_usage_clientauth_serverauth_timestamping));

  REQUIRE_FALSE(key_usage_clientauth.isSubsetOf(key_usage_empty));
  REQUIRE(key_usage_clientauth.isSubsetOf(key_usage_clientauth));
  REQUIRE(key_usage_clientauth.isSubsetOf(key_usage_clientauth_serverauth));
  REQUIRE(key_usage_clientauth.isSubsetOf(key_usage_clientauth_serverauth_codesigning));
  REQUIRE(key_usage_clientauth.isSubsetOf(key_usage_clientauth_serverauth_timestamping));

  REQUIRE_FALSE(key_usage_clientauth_serverauth.isSubsetOf(key_usage_empty));
  REQUIRE_FALSE(key_usage_clientauth_serverauth.isSubsetOf(key_usage_clientauth));
  REQUIRE(key_usage_clientauth_serverauth.isSubsetOf(key_usage_clientauth_serverauth));
  REQUIRE(key_usage_clientauth_serverauth.isSubsetOf(key_usage_clientauth_serverauth_codesigning));
  REQUIRE(key_usage_clientauth_serverauth.isSubsetOf(key_usage_clientauth_serverauth_timestamping));

  REQUIRE_FALSE(key_usage_clientauth_serverauth_codesigning.isSubsetOf(key_usage_empty));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_codesigning.isSubsetOf(key_usage_clientauth));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_codesigning.isSubsetOf(key_usage_clientauth_serverauth));
  REQUIRE(key_usage_clientauth_serverauth_codesigning.isSubsetOf(key_usage_clientauth_serverauth_codesigning));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_codesigning.isSubsetOf(key_usage_clientauth_serverauth_timestamping));

  REQUIRE_FALSE(key_usage_clientauth_serverauth_timestamping.isSubsetOf(key_usage_empty));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_timestamping.isSubsetOf(key_usage_clientauth));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_timestamping.isSubsetOf(key_usage_clientauth_serverauth));
  REQUIRE_FALSE(key_usage_clientauth_serverauth_timestamping.isSubsetOf(key_usage_clientauth_serverauth_codesigning));
  REQUIRE(key_usage_clientauth_serverauth_timestamping.isSubsetOf(key_usage_clientauth_serverauth_timestamping));
}

}  // namespace

TEST_CASE("ExtendedKeyUsage can be created from an ASN.1 structure", "[constructor][isSubsetOf]") {
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage_ptr_empty = createExtendedKeyUsage({});
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage_ptr_clientauth = createExtendedKeyUsage({2});
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage_ptr_clientauth_serverauth = createExtendedKeyUsage({2, 1});
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage_ptr_clientauth_serverauth_codesigning = createExtendedKeyUsage({2, 1, 3});
  utils::tls::EXTENDED_KEY_USAGE_unique_ptr key_usage_ptr_clientauth_serverauth_timestamping = createExtendedKeyUsage({2, 1, 8});

  utils::tls::ExtendedKeyUsage key_usage_empty{*key_usage_ptr_empty};
  utils::tls::ExtendedKeyUsage key_usage_clientauth{*key_usage_ptr_clientauth};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth{*key_usage_ptr_clientauth_serverauth};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth_codesigning{*key_usage_ptr_clientauth_serverauth_codesigning};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth_timestamping{*key_usage_ptr_clientauth_serverauth_timestamping};

  testIsSubsetOf(key_usage_empty,
                 key_usage_clientauth,
                 key_usage_clientauth_serverauth,
                 key_usage_clientauth_serverauth_codesigning,
                 key_usage_clientauth_serverauth_timestamping);
}

TEST_CASE("ExtendedKeyUsage can be created from list of usage strings", "[constructor][isSubsetOf]") {
  constexpr const char* key_usage_str_empty = "";
  constexpr const char* key_usage_str_clientauth = "Client Authentication";
  constexpr const char* key_usage_str_clientauth_serverauth = "Client Authentication, Server Authentication";
  constexpr const char* key_usage_str_clientauth_serverauth_codesigning = "Server Authentication, Client Authentication,Code Signing";
  constexpr const char* key_usage_str_clientauth_serverauth_timestamping = "\tClient Authentication , Time Stamping\t,\tServer Authentication\r";

  utils::tls::ExtendedKeyUsage key_usage_empty{key_usage_str_empty};
  utils::tls::ExtendedKeyUsage key_usage_clientauth{key_usage_str_clientauth};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth{key_usage_str_clientauth_serverauth};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth_codesigning{key_usage_str_clientauth_serverauth_codesigning};
  utils::tls::ExtendedKeyUsage key_usage_clientauth_serverauth_timestamping{key_usage_str_clientauth_serverauth_timestamping};

  testIsSubsetOf(key_usage_empty,
                 key_usage_clientauth,
                 key_usage_clientauth_serverauth,
                 key_usage_clientauth_serverauth_codesigning,
                 key_usage_clientauth_serverauth_timestamping);
}

TEST_CASE("ExtendedKeyUsage created from ASN.1 and string are identical", "[constructor][equal]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({})} == utils::tls::ExtendedKeyUsage{""});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({2})} == utils::tls::ExtendedKeyUsage{"Client Authentication"});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2})} == utils::tls::ExtendedKeyUsage{"Client Authentication, Server Authentication"});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2, 4})} == utils::tls::ExtendedKeyUsage{"Client Authentication, Server Authentication, Secure Email"});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({8, 9})} == utils::tls::ExtendedKeyUsage{"Time Stamping, OCSP Signing"});
}

TEST_CASE("ExtendedKeyUsage objects with the same flags in a different order are equal", "[constructor][equal]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{"Server Authentication, Client Authentication"} == utils::tls::ExtendedKeyUsage{"Client Authentication, Server Authentication"});

  utils::tls::ExtendedKeyUsage key_usage_123{"Server Authentication, Client Authentication, Code Signing"};
  utils::tls::ExtendedKeyUsage key_usage_132{"Server Authentication, Code Signing, Client Authentication"};
  utils::tls::ExtendedKeyUsage key_usage_213{"Client Authentication, Server Authentication, Code Signing"};
  utils::tls::ExtendedKeyUsage key_usage_231{"Client Authentication, Code Signing, Server Authentication"};
  utils::tls::ExtendedKeyUsage key_usage_312{"Code Signing, Server Authentication, Client Authentication"};
  utils::tls::ExtendedKeyUsage key_usage_321{"Code Signing, Client Authentication, Server Authentication"};
  REQUIRE(key_usage_123 == key_usage_132);
  REQUIRE(key_usage_123 == key_usage_213);
  REQUIRE(key_usage_123 == key_usage_231);
  REQUIRE(key_usage_123 == key_usage_312);
  REQUIRE(key_usage_123 == key_usage_321);
}

TEST_CASE("ExtendedKeyUsage objects with different flags are different", "[constructor][not_equal]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{""} != utils::tls::ExtendedKeyUsage{"Client Authentication"});
  REQUIRE(utils::tls::ExtendedKeyUsage{"Client Authentication"} != utils::tls::ExtendedKeyUsage{"Client Authentication, Server Authentication"});
  REQUIRE(utils::tls::ExtendedKeyUsage{"Client Authentication"} != utils::tls::ExtendedKeyUsage{"Code Signing"});
}

TEST_CASE("ExtendedKeyUsage ignores unrecognized string flags", "[constructor]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{""} == utils::tls::ExtendedKeyUsage{"Rubik's Cube Shuffling"});
  REQUIRE(utils::tls::ExtendedKeyUsage{"Client Authentication"} == utils::tls::ExtendedKeyUsage{"Client Authentication, Client Onboarding"});
  REQUIRE(utils::tls::ExtendedKeyUsage{"Client Authentication, Server Authentication"} == utils::tls::ExtendedKeyUsage{"User Authentication, Server Authentication, Client Authentication"});
}

TEST_CASE("ExtendedKeyUsage can set unrecognized ASN.1 flags", "[constructor]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({})} != utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({10})});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1})} != utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 12})});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2})} != utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2, 15})});
}

TEST_CASE("ExtendedKeyUsage ignores ASN.1 flags higher than 15", "[constructor]") {
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({})} == utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({16})});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1})} == utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 100})});
  REQUIRE(utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2})} == utils::tls::ExtendedKeyUsage{*createExtendedKeyUsage({1, 2, 127})});
  // values with the first bit set cause an error:
  REQUIRE(createExtendedKeyUsage({128}) == nullptr);
  REQUIRE(createExtendedKeyUsage({1, 2, 200}) == nullptr);
}

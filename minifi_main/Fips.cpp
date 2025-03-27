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
#include "Fips.h"

#include <fstream>
#include <algorithm>
#include <string>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "utils/Environment.h"
#include "utils/StringUtils.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::fips {

namespace {
#ifdef WIN32
constexpr std::string_view FIPS_LIB = "fips.dll";
#elif defined(__APPLE__)
constexpr std::string_view FIPS_LIB = "fips.dylib";
#else
constexpr std::string_view FIPS_LIB = "fips.so";
#endif

bool replaceMinifiHomeVariable(const std::filesystem::path& file_path, const std::filesystem::path& minifi_home_path, const std::shared_ptr<core::logging::Logger>& logger) {
  std::ifstream input_file(file_path);
  if (!input_file) {
    logger->log_error("Failed to open file: {}", file_path.string());
    return false;
  }

  std::ostringstream buffer;
  buffer << input_file.rdbuf();
  std::string content = buffer.str();
  input_file.close();

  const std::string placeholder = "${MINIFI_HOME}";
  size_t pos = content.find(placeholder, 0);
  if (pos == std::string::npos) {
    return true;
  }

  auto minifi_home_path_str = minifi_home_path.generic_string();
  do {
    content.replace(pos, placeholder.length(), minifi_home_path_str);
    pos += minifi_home_path_str.length();
  } while((pos = content.find(placeholder, pos)) != std::string::npos);

  std::ofstream output_file(file_path);
  if (!output_file) {
    logger->log_error("Failed to open file for writing: {}", file_path.string());
    return false;
  }

  output_file << content;
  output_file.close();
  return true;
}

bool generateFipsModuleConfig(const std::filesystem::path& minifi_home, const std::shared_ptr<core::logging::Logger>& logger) {
  std::filesystem::path output_file(minifi_home / "fips" / "fipsmodule.cnf");
  logger->log_info("fipsmodule.cnf was not found, trying to run fipsinstall command to generate the file");

#ifdef WIN32
  std::string command = "\"\"" + (minifi_home / "fips" / "openssl.exe").string() + "\" fipsinstall -out \"" + output_file.string() + "\" -module \"" + (minifi_home / "fips" / FIPS_LIB).string() + "\"\"";
#else
  std::string command = "\"" + (minifi_home / "fips" / "openssl").string() + "\" fipsinstall -out \"" + output_file.string() + "\" -module \"" + (minifi_home / "fips" / FIPS_LIB).string() + "\"";
#endif
  auto ret = std::system(command.c_str());
  if (ret != 0) {
    logger->log_error("Failed to generate fipsmodule.cnf file");
    return false;
  }
  logger->log_info("Successfully generated fipsmodule.cnf file");
  return true;
}
}  // namespace

void initializeFipsMode(const std::shared_ptr<minifi::Configure>& configure, const std::filesystem::path& minifi_home, const std::shared_ptr<core::logging::Logger>& logger) {
  if (!(configure->get(minifi::Configure::nifi_openssl_fips_support_enable) | utils::andThen(utils::string::toBool)).value_or(false)) {
    return;
  }

  if (!std::filesystem::exists(minifi_home / "fips" / FIPS_LIB)) {
    logger->log_error("FIPS mode is enabled, but {} is not available in MINIFI_HOME/fips directory", FIPS_LIB);
    std::exit(1);
  }

  if (!std::filesystem::exists(minifi_home / "fips" / "fipsmodule.cnf") && !generateFipsModuleConfig(minifi_home, logger)) {
    logger->log_error("FIPS mode is enabled, but fipsmodule.cnf is not available in $MINIFI_HOME/fips directory, and minifi couldn't generate it automatically.  "
      "Run $MINIFI_HOME/fips/openssl fipsinstall -out fipsmodule.cnf -module $MINIFI_HOME/fips/{} command to generate the configuration file", FIPS_LIB);
    std::exit(1);
  }

  if (!std::filesystem::exists(minifi_home / "fips" / "openssl.cnf")) {
    logger->log_error("FIPS mode is enabled, but openssl.cnf is not available in MINIFI_HOME/fips directory");
    std::exit(1);
  }

  if (!replaceMinifiHomeVariable(minifi_home / "fips" / "openssl.cnf", minifi_home, logger)) {
    logger->log_error("Failed to replace MINIFI_HOME variable in openssl.cnf");
    std::exit(1);
  }

  utils::Environment::setEnvironmentVariable("OPENSSL_CONF", (minifi_home / "fips" / "openssl.cnf").string().c_str(), true);

  if (!OSSL_PROVIDER_set_default_search_path(nullptr, (minifi_home / "fips").string().c_str())) {
    logger->log_error("Failed to set FIPS module path: {}", (minifi_home / "fips").string());
    ERR_print_errors_fp(stderr);
    std::exit(1);
  }

  if (OSSL_PROVIDER_available(nullptr, "fips") != 1) {
    logger->log_error("FIPS provider not available in default search path");
    ERR_print_errors_fp(stderr);
    std::exit(1);
  }

  if (!EVP_default_properties_enable_fips(nullptr, 1)) {
    logger->log_error("Failed to enable FIPS mode");
    ERR_print_errors_fp(stderr);
    std::exit(1);
  }

  if (!EVP_default_properties_is_fips_enabled(nullptr)) {
    logger->log_error("FIPS mode is not enabled");
    ERR_print_errors_fp(stderr);
    std::exit(1);
  }

  logger->log_info("FIPS mode enabled in MiNiFi C++");
}

}  // namespace org::apache::nifi::minifi::fips

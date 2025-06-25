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

bool substituteFipsDirVariable(const std::filesystem::path& file_path, const std::filesystem::path& fips_dir_path, const std::shared_ptr<core::logging::Logger>& logger) {
  std::ifstream input_file(file_path);
  if (!input_file) {
    logger->log_error("Failed to open file: {}", file_path.string());
    return false;
  }

  std::ostringstream buffer;
  buffer << input_file.rdbuf();
  std::string content = buffer.str();
  input_file.close();

  const std::string placeholder = "${FIPS_DIR}";
  size_t pos = content.find(placeholder, 0);
  if (pos == std::string::npos) {
    return true;
  }

  auto fips_dir_str = fips_dir_path.generic_string();
  do {
    content.replace(pos, placeholder.length(), fips_dir_str);
    pos += fips_dir_str.length();
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

bool generateFipsModuleConfig(const std::filesystem::path& fips_dir, const std::shared_ptr<core::logging::Logger>& logger) {
  std::filesystem::path output_file(fips_dir / "fipsmodule.cnf");
  logger->log_info("fipsmodule.cnf was not found, trying to run fipsinstall command to generate the file");

#ifdef WIN32
  std::string command = "\"\"" + (fips_dir / "openssl.exe").string() + "\" fipsinstall -out \"" + output_file.string() + "\" -module \"" + (fips_dir / FIPS_LIB).string() + "\"\"";
#else
  std::string command = "\"" + (fips_dir / "openssl").string() + "\" fipsinstall -out \"" + output_file.string() + "\" -module \"" + (fips_dir / FIPS_LIB).string() + "\"";
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

void initializeFipsMode(const std::shared_ptr<minifi::Configure>& configure, const std::filesystem::path& fips_dir, const std::shared_ptr<core::logging::Logger>& logger) {
  if (!(configure->get(minifi::Configure::nifi_openssl_fips_support_enable) | utils::andThen(utils::string::toBool)).value_or(false)) {
    logger->log_info("FIPS mode is disabled. FIPS configs and modules will NOT be loaded.");
    return;
  }

  if (!std::filesystem::exists(fips_dir / FIPS_LIB)) {
    logger->log_error("FIPS mode is enabled, but {} is not available in {} directory", FIPS_LIB, fips_dir);
    std::exit(1);
  }

  if (!std::filesystem::exists(fips_dir / "fipsmodule.cnf") && !generateFipsModuleConfig(fips_dir, logger)) {
    logger->log_error("FIPS mode is enabled, but fipsmodule.cnf is not available in {} directory, and minifi couldn't generate it automatically.  "
      "Run {}/openssl fipsinstall -out fipsmodule.cnf -module {}/{} command to generate the configuration file", fips_dir, fips_dir, FIPS_LIB);
    std::exit(1);
  }

  if (!std::filesystem::exists(fips_dir / "openssl.cnf")) {
    logger->log_error("FIPS mode is enabled, but openssl.cnf is not available in {} directory", fips_dir);
    std::exit(1);
  }

  if (!substituteFipsDirVariable(fips_dir / "openssl.cnf", fips_dir, logger)) {
    logger->log_error("Failed to replace FIPS_DIR variable in openssl.cnf");
    std::exit(1);
  }

  utils::Environment::setEnvironmentVariable("OPENSSL_CONF", (fips_dir / "openssl.cnf").string().c_str(), true);

  if (!OSSL_PROVIDER_set_default_search_path(nullptr, (fips_dir).string().c_str())) {
    logger->log_error("Failed to set FIPS module path: {}", (fips_dir).string());
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

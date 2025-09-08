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

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/provider.h>

#include <fstream>
#include <string>

#include "MainHelper.h"
#include "utils/Environment.h"
#include "utils/OptionalUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::fips {

namespace {
#ifdef WIN32
constexpr std::string_view FIPS_LIB = "fips.dll";
#elif defined(__APPLE__)
constexpr std::string_view FIPS_LIB = "fips.dylib";
#else
constexpr std::string_view FIPS_LIB = "fips.so";
#endif

bool substituteFipsDirVariable(const std::filesystem::path& file_path, const std::filesystem::path& fips_conf_dir, const std::shared_ptr<core::logging::Logger>& logger) {
  std::ifstream input_file(file_path);
  if (!input_file) {
    logger->log_error("Failed to open file: {}", file_path.string());
    return false;
  }

  std::ostringstream buffer;
  buffer << input_file.rdbuf();
  std::string content = buffer.str();
  input_file.close();

  const std::string placeholder = "${FIPS_CONF_DIR}";
  size_t pos = content.find(placeholder, 0);
  if (pos == std::string::npos) {
    return true;
  }

  auto fips_dir_str = fips_conf_dir.generic_string();
  do {
    content.replace(pos, placeholder.length(), fips_dir_str);
    pos += fips_dir_str.length();
  } while ((pos = content.find(placeholder, pos)) != std::string::npos);

  std::ofstream output_file(file_path);
  if (!output_file) {
    logger->log_error("Failed to open file for writing: {}", file_path.string());
    return false;
  }

  output_file << content;
  output_file.close();
  return true;
}

bool generateFipsModuleConfig(const Locations& locations, const std::shared_ptr<core::logging::Logger>& logger) {
  const auto& fips_bin_path = locations.fips_bin_path_;
  const auto& fips_conf_path = locations.fips_conf_path_;
  std::filesystem::path output_file(fips_conf_path / "fipsmodule.cnf");
  logger->log_info("fipsmodule.cnf was not found, trying to run fipsinstall command to generate the file");

#ifdef WIN32
  std::string command = fmt::format(R"("{}" fipsinstall -out "{}" -module "{}")", fips_bin_path / "openssl.exe", output_file, fips_bin_path / FIPS_LIB);
#else
  std::string command = fmt::format(R"("{}" fipsinstall -out "{}" -module "{}")", fips_bin_path / "openssl", output_file, fips_bin_path / FIPS_LIB);
#endif
  if (std::system(command.c_str()) != 0) {
    logger->log_error("Failed to generate fipsmodule.cnf file");
    return false;
  }
  logger->log_info("Successfully generated fipsmodule.cnf file");
  return true;
}
}  // namespace

void initializeFipsMode(const std::shared_ptr<minifi::Configure>& configure, const Locations& locations, const std::shared_ptr<core::logging::Logger>& logger) {
  const auto& fips_bin_path = locations.fips_bin_path_;
  const auto& fips_conf_path = locations.fips_conf_path_;
  if (!(configure->get(minifi::Configure::nifi_openssl_fips_support_enable) | utils::andThen(utils::string::toBool)).value_or(false)) {
    logger->log_info("FIPS mode is disabled. FIPS configs and modules will NOT be loaded.");
    return;
  }

  if (!std::filesystem::exists(fips_bin_path / FIPS_LIB)) {
    logger->log_error("FIPS mode is enabled, but {} is not available in {} directory", FIPS_LIB, fips_bin_path);
    std::exit(1);
  }

  if (!std::filesystem::exists(fips_conf_path / "fipsmodule.cnf") && !generateFipsModuleConfig(locations, logger)) {
    logger->log_error("FIPS mode is enabled, but fipsmodule.cnf is not available in {fips_conf_dir} directory, and minifi couldn't generate it automatically.  "
      "Run {fips_bin_dir}/openssl fipsinstall -out {fips_conf_dir}/fipsmodule.cnf -module {fips_bin_dir}/{fips_lib_name} command to generate the configuration file",
      fmt::arg("fips_conf_dir", fips_conf_path),
      fmt::arg("fips_bin_dir", fips_bin_path),
      fmt::arg("fips_lib_name", FIPS_LIB));
    std::exit(1);
  }

  if (!std::filesystem::exists(fips_conf_path / "openssl.cnf")) {
    logger->log_error("FIPS mode is enabled, but openssl.cnf is not available in {} directory", fips_conf_path);
    std::exit(1);
  }

  if (!substituteFipsDirVariable(fips_conf_path / "openssl.cnf", fips_conf_path, logger)) {
    logger->log_error("Failed to replace FIPS_CONF_DIR variable in openssl.cnf");
    std::exit(1);
  }

  utils::Environment::setEnvironmentVariable("OPENSSL_CONF", (fips_conf_path / "openssl.cnf").string().c_str(), true);

  if (!OSSL_PROVIDER_set_default_search_path(nullptr, (fips_bin_path.string()).c_str())) {
    logger->log_error("Failed to set FIPS module path: {}", fips_bin_path.string());
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

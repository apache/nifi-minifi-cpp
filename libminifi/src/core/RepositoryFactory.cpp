// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "core/RepositoryFactory.h"
#include <memory>
#include <string>
#include <algorithm>
#include "core/repository/VolatileContentRepository.h"
#include "core/ClassLoader.h"
#include "core/repository/FileSystemRepository.h"
#include "core/repository/VolatileProvenanceRepository.h"
#include "core/repository/NoOpThreadedRepository.h"
#include "range/v3/algorithm/transform.hpp"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

std::unique_ptr<ContentRepository> createContentRepository(const std::string& configuration_class_name,
    const std::string& repo_name,
    logging::Logger& logger) {
  std::string class_name_lc = configuration_class_name;
  ranges::transform(class_name_lc, class_name_lc.begin(), ::tolower);

  if (auto return_obj = ClassLoader::getDefaultClassLoader().instantiate<ContentRepository>(class_name_lc, class_name_lc)) {
    return_obj->setName(repo_name);
    return return_obj;
  }

  if (class_name_lc == "volatilecontentrepository") {
    return std::make_unique<repository::VolatileContentRepository>(repo_name);
  }
  if (class_name_lc == "filesystemrepository") {
    return std::make_unique<repository::FileSystemRepository>(repo_name);
  }

  logger.log_critical("Could not create the configured content repository ({})", configuration_class_name);
  if (class_name_lc == "databasecontentrepository") {
    logger.log_error("To use DatabaseContentRepository MiNiFi needs RocksDB extension, please check the extension path configured in minifi.properties");
  } else if (class_name_lc == "lmdbcontentrepository") {
    logger.log_error("To use LmdbContentRepository MiNiFi needs LMDB extension, please check the extension path configured in minifi.properties");
  }

  throw std::runtime_error("Support for the provided configuration class could not be found, check logs for more details");
}

std::unique_ptr<Repository> createRepository(const std::string& configuration_class_name, const std::string& repo_name) {
  std::string class_name_lc = configuration_class_name;
  std::transform(class_name_lc.begin(), class_name_lc.end(), class_name_lc.begin(), ::tolower);
  auto return_obj = ClassLoader::getDefaultClassLoader().instantiate<ThreadedRepository>(class_name_lc, class_name_lc);
  if (return_obj) {
    return_obj->setName(repo_name);
    return return_obj;
  }
  // if the desired repos don't exist, we can try doing string matches and rely on volatile repositories
  if (class_name_lc == "flowfilerepository" || class_name_lc == "volatileflowfilerepository" || class_name_lc == "nooprepository") {
    return std::make_unique<repository::NoOpThreadedRepository>(repo_name);
  }
  if (class_name_lc == "provenancerepository" || class_name_lc == "volatileprovenancerepository") {
    return instantiate<repository::VolatileProvenanceRepository>(repo_name);
  }
  return {};
}

}  // namespace org::apache::nifi::minifi::core

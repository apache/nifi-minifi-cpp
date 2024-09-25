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

#include "KubernetesControllerService.h"

#include <vector>

extern "C" {
#include "api/CoreV1API.h"
}

#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "Exception.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::controllers {

KubernetesControllerService::KubernetesControllerService(const std::string& name, const utils::Identifier& uuid)
  : AttributeProviderService(name, uuid),
    logger_{core::logging::LoggerFactory<KubernetesControllerService>::getLogger(uuid)} {
}

KubernetesControllerService::KubernetesControllerService(const std::string& name, const std::shared_ptr<Configure>& configuration)
  : KubernetesControllerService{name} {
    setConfiguration(configuration);
    initialize();
}

void KubernetesControllerService::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) { return; }

  ControllerServiceImpl::initialize();
  setSupportedProperties(Properties);
  initialized_ = true;
}

void KubernetesControllerService::onEnable() {
  try {
    api_client_ = std::make_unique<kubernetes::ApiClient>();
  } catch (const std::runtime_error& ex) {
    logger_->log_error("Could not create the API client in the Kubernetes Controller Service: {}", ex.what());
  }

  std::string namespace_filter;
  if (getProperty(NamespaceFilter, namespace_filter) && !namespace_filter.empty()) {
    namespace_filter_ = utils::Regex{namespace_filter};
  }

  std::string pod_name_filter;
  if (getProperty(PodNameFilter, pod_name_filter) && !pod_name_filter.empty()) {
    pod_name_filter_ = utils::Regex{pod_name_filter};
  }

  std::string container_name_filter;
  if (getProperty(ContainerNameFilter, container_name_filter) && !container_name_filter.empty()) {
    container_name_filter_ = utils::Regex{container_name_filter};
  }
}

namespace {

struct v1_pod_list_t_deleter {
  void operator()(v1_pod_list_t* ptr) const noexcept { v1_pod_list_free(ptr); }
};
using v1_pod_list_unique_ptr = std::unique_ptr<v1_pod_list_t, v1_pod_list_t_deleter>;

v1_pod_list_unique_ptr getPods(gsl::not_null<apiClient_t*> api_client, core::logging::Logger& logger) {
  logger.log_info("Calling Kubernetes API listPodForAllNamespaces...");
  v1_pod_list_unique_ptr pod_list{CoreV1API_listPodForAllNamespaces(api_client,
                                                                    0,  // allowWatchBookmarks
                                                                    nullptr,  // continue
                                                                    nullptr,  // fieldSelector
                                                                    nullptr,  // labelSelector
                                                                    0,  // limit
                                                                    nullptr,  // pretty
                                                                    nullptr,  // resourceVersion
                                                                    nullptr,  // resourceVersionMatch
                                                                    0,  // timeoutSeconds
                                                                    0)};  // watch
  logger.log_info("The return code of the Kubernetes API listPodForAllNamespaces call: {}", api_client->response_code);
  return pod_list;
}

}  // namespace

std::optional<std::vector<KubernetesControllerService::AttributeMap>> KubernetesControllerService::getAttributes() {
  if (!api_client_) {
    logger_->log_warn("The Kubernetes client is not valid, unable to call the Kubernetes API");
    return std::nullopt;
  }

  const auto pod_list = getPods(api_client_->getClient(), *logger_);
  if (!pod_list) {
    logger_->log_warn("Could not find any Kubernetes pods");
    return std::nullopt;
  }

  std::vector<AttributeMap> container_attribute_maps;

  listEntry_t* pod_entry = nullptr;
  list_ForEach(pod_entry, pod_list->items) {
    const auto pod = static_cast<v1_pod_t*>(pod_entry->data);

    std::string name_space{pod->metadata->_namespace};
    std::string pod_name{pod->metadata->name};
    std::string uid{pod->metadata->uid};

    listEntry_t* container_entry = nullptr;
    list_ForEach(container_entry, pod->spec->containers) {
      auto container = static_cast<v1_container_t*>(container_entry->data);

      std::string container_name{container->name};

      if (matchesRegexFilters(name_space, pod_name, container_name)) {
        container_attribute_maps.push_back(AttributeMap{
            {"namespace", name_space},
            {"pod", pod_name},
            {"uid", uid},
            {"container", container_name}});
      }
    }
  }

  logger_->log_info("Found {} containers (after regex filtering) in {} Kubernetes pods (unfiltered)", container_attribute_maps.size(), pod_list->items->count);
  return container_attribute_maps;
}

bool KubernetesControllerService::matchesRegexFilters(const kubernetes::ContainerInfo& container_info) const {
  return matchesRegexFilters(container_info.name_space, container_info.pod_name, container_info.container_name);
}

bool KubernetesControllerService::matchesRegexFilters(const std::string& name_space, const std::string& pod_name, const std::string& container_name) const {
  static constexpr auto matchesFilter = [](const std::string& target, const std::optional<utils::Regex>& filter) {
    return !filter || utils::regexMatch(target, *filter);
  };
  return matchesFilter(name_space, namespace_filter_) &&
      matchesFilter(pod_name, pod_name_filter_) &&
      matchesFilter(container_name, container_name_filter_);
}

REGISTER_RESOURCE(KubernetesControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers

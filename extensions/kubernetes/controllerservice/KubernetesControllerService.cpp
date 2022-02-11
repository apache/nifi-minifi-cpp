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
#include "config/incluster_config.h"
#include "config/kube_config.h"
#include "include/apiClient.h"
#include "api/CoreV1API.h"
}

#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "Exception.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::controllers {

class KubernetesControllerService::APIClient {
 public:
  explicit APIClient(core::logging::Logger& logger);
  ~APIClient();

  APIClient(APIClient&&) = delete;
  APIClient(const APIClient&) = delete;
  APIClient& operator=(APIClient&&) = delete;
  APIClient& operator=(const APIClient&) = delete;

  [[nodiscard]] apiClient_t* getClient() const noexcept { return api_client_; }

 private:
  char* base_path_ = nullptr;
  sslConfig_t* ssl_config_ = nullptr;
  list_t* api_keys_ = nullptr;
  apiClient_t* api_client_ = nullptr;
};

KubernetesControllerService::APIClient::APIClient(core::logging::Logger& logger) {
  int rc = load_incluster_config(&base_path_, &ssl_config_, &api_keys_);
  if (rc != 0) {
    logger.log_error("Cannot load kubernetes configuration in cluster");
    return;
  }
  api_client_ = apiClient_create_with_base_path(base_path_, ssl_config_, api_keys_);
  if (!api_client_) {
    logger.log_error("Cannot create a kubernetes client");
  }
}

KubernetesControllerService::APIClient::~APIClient() {
  if (api_client_) {
    apiClient_free(api_client_);
    api_client_ = nullptr;
  }

  free_client_config(base_path_, ssl_config_, api_keys_);
  base_path_ = nullptr;
  ssl_config_ = nullptr;
  api_keys_ = nullptr;

  apiClient_unsetupGlobalEnv();
}

const core::Property KubernetesControllerService::NamespaceFilter{
    core::PropertyBuilder::createProperty("Namespace Filter")
        ->withDescription("Limit the output to pods in namespaces which match this regular expression")
        ->withDefaultValue<std::string>("default")
        ->build()};
const core::Property KubernetesControllerService::PodNameFilter{
    core::PropertyBuilder::createProperty("Pod Name Filter")
        ->withDescription("If present, limit the output to pods the name of which matches this regular expression")
        ->build()};
const core::Property KubernetesControllerService::ContainerNameFilter{
    core::PropertyBuilder::createProperty("Container Name Filter")
        ->withDescription("If present, limit the output to containers the name of which matches this regular expression")
        ->build()};

KubernetesControllerService::KubernetesControllerService(const std::string& name, const utils::Identifier& uuid)
  : AttributeProviderService(name, uuid),
    logger_{core::logging::LoggerFactory<KubernetesControllerService>::getLogger()},
    api_client_{std::make_unique<APIClient>(*logger_)} {
}

KubernetesControllerService::KubernetesControllerService(const std::string& name, const std::shared_ptr<Configure>& configuration)
  : KubernetesControllerService{name} {
    setConfiguration(configuration);
    initialize();
}

void KubernetesControllerService::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) { return; }

  ControllerService::initialize();
  setSupportedProperties({NamespaceFilter, PodNameFilter, ContainerNameFilter});
  initialized_ = true;
}

void KubernetesControllerService::onEnable() {
  std::string namespace_filter;
  if (getProperty(NamespaceFilter.getName(), namespace_filter) && !namespace_filter.empty()) {
    namespace_filter_ = std::regex{namespace_filter};
  }

  std::string pod_name_filter;
  if (getProperty(PodNameFilter.getName(), pod_name_filter) && !pod_name_filter.empty()) {
    pod_name_filter_ = std::regex{pod_name_filter};
  }

  std::string container_name_filter;
  if (getProperty(ContainerNameFilter.getName(), container_name_filter) && !container_name_filter.empty()) {
    container_name_filter_ = std::regex{container_name_filter};
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
  logger.log_info("The return code of the Kubernetes API listPodForAllNamespaces call: %ld", api_client->response_code);
  return pod_list;
}

}  // namespace

std::optional<std::vector<KubernetesControllerService::AttributeMap>> KubernetesControllerService::getAttributes() {
  if (!api_client_->getClient()) {
    logger_->log_warn("The Kubernetes client is not valid, unable to call the Kubernetes API");
    return std::nullopt;
  }

  const auto pod_list = getPods(gsl::make_not_null(api_client_->getClient()), *logger_);
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

  logger_->log_info("Found %zu containers (after regex filtering) in %ld Kubernetes pods (unfiltered)", container_attribute_maps.size(), pod_list->items->count);
  return container_attribute_maps;
}

bool KubernetesControllerService::matchesRegexFilters(const std::string& name_space, const std::string& pod_name, const std::string& container_name) const {
  static constexpr auto matchesFilter = [](const std::string& target, const std::optional<std::regex>& filter) {
    return !filter || std::regex_match(target, *filter);
  };
  return matchesFilter(name_space, namespace_filter_) &&
      matchesFilter(pod_name, pod_name_filter_) &&
      matchesFilter(container_name, container_name_filter_);
}

REGISTER_RESOURCE(KubernetesControllerService, "Controller service that provides access to the Kubernetes API");

}  // namespace org::apache::nifi::minifi::controllers

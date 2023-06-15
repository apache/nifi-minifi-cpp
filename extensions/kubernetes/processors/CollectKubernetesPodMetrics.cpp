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
#include "CollectKubernetesPodMetrics.h"

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "../ContainerInfo.h"
#include "../MetricsApi.h"
#include "../MetricsFilter.h"

namespace org::apache::nifi::minifi::processors {

void CollectKubernetesPodMetrics::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void CollectKubernetesPodMetrics::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>&) {
  gsl_Expects(context);

  const auto controller_service_name = context->getProperty(KubernetesControllerService);
  if (!controller_service_name || controller_service_name->empty()) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::StringUtils::join_pack("Missing '", KubernetesControllerService.name, "' property")};
  }

  std::shared_ptr<core::controller::ControllerService> controller_service = context->getControllerService(*controller_service_name);
  if (!controller_service) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::StringUtils::join_pack("Controller service '", *controller_service_name, "' not found")};
  }

  kubernetes_controller_service_ = std::dynamic_pointer_cast<minifi::controllers::KubernetesControllerService>(controller_service);
  if (!kubernetes_controller_service_) {
    throw minifi::Exception{ExceptionType::PROCESS_SCHEDULE_EXCEPTION, utils::StringUtils::join_pack("Controller service '", *controller_service_name, "' is not a KubernetesControllerService")};
  }
}

void CollectKubernetesPodMetrics::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && kubernetes_controller_service_);

  const kubernetes::ApiClient* api_client = kubernetes_controller_service_->apiClient();
  if (!api_client) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "The KubernetesControllerService is in a bad state: the API client is null");
  }

  const auto metrics = kubernetes::metrics::podMetricsList(*api_client);
  if (!metrics) {
    logger_->log_error("Could not get metrics from the Kubernetes API: %s", metrics.error());
    return;
  }

  const auto metrics_filtered = kubernetes::metrics::filter(metrics.value(), [this](const kubernetes::ContainerInfo& container_info) {
    return kubernetes_controller_service_->matchesRegexFilters(container_info);
  });
  if (!metrics_filtered) {
    logger_->log_error("Error parsing or filtering the metrics received from the Kubernetes API: %s", metrics_filtered.error());
    return;
  }

  logger_->log_debug("Metrics received from the Kubernetes API: %s", metrics_filtered.value());

  const auto flow_file = session->create();
  session->writeBuffer(flow_file, metrics_filtered.value());
  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(CollectKubernetesPodMetrics, Processor);

}  // namespace org::apache::nifi::minifi::processors

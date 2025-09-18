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

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "../ContainerInfo.h"
#include "../MetricsApi.h"
#include "../MetricsFilter.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void CollectKubernetesPodMetrics::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void CollectKubernetesPodMetrics::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  kubernetes_controller_service_ = utils::parseControllerService<controllers::KubernetesControllerService>(context, KubernetesControllerService, getUUID());
}

void CollectKubernetesPodMetrics::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  gsl_Expects(kubernetes_controller_service_);

  const kubernetes::ApiClient* api_client = kubernetes_controller_service_->apiClient();
  if (!api_client) {
    throw minifi::Exception(ExceptionType::PROCESSOR_EXCEPTION, "The KubernetesControllerService is in a bad state: the API client is null");
  }

  const auto metrics = kubernetes::metrics::podMetricsList(*api_client);
  if (!metrics) {
    logger_->log_error("Could not get metrics from the Kubernetes API: {}", metrics.error());
    return;
  }

  const auto metrics_filtered = kubernetes::metrics::filter(metrics.value(), [this](const kubernetes::ContainerInfo& container_info) {
    return kubernetes_controller_service_->matchesRegexFilters(container_info);
  });
  if (!metrics_filtered) {
    logger_->log_error("Error parsing or filtering the metrics received from the Kubernetes API: {}", metrics_filtered.error());
    return;
  }

  logger_->log_debug("Metrics received from the Kubernetes API: {}", metrics_filtered.value());

  const auto flow_file = session.create();
  session.writeBuffer(flow_file, metrics_filtered.value());
  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(CollectKubernetesPodMetrics, Processor);

}  // namespace org::apache::nifi::minifi::processors

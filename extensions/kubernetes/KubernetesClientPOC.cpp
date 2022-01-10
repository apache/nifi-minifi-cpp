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

#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string>

extern "C" {
#include "config/incluster_config.h"
#include "config/kube_config.h"
#include "include/apiClient.h"
#include "api/CoreV1API.h"
}

void list_pod(apiClient_t *apiClient) {
  v1_pod_list_t *pod_list = NULL;
  std::string name_space = "default";
  std::string field_selector = "spec.nodeName=kind-control-plane";
  pod_list = CoreV1API_listNamespacedPod(apiClient,
                                         name_space.data(),
                                         NULL,    /* pretty */
                                         0,   /* allowWatchBookmarks */
                                         NULL,    /* continue */
                                         field_selector.data(),
                                         NULL,    /* labelSelector */
                                         0,   /* limit */
                                         NULL,    /* resourceVersion */
                                         NULL,    /* resourceVersionMatch */
                                         0,   /* timeoutSeconds */
                                         0    /* watch */);
  printf("The return code of HTTP request=%ld\n", apiClient->response_code);
  if (pod_list) {
    printf("Get pod list:\n");
    listEntry_t *listEntry = NULL;
    v1_pod_t *pod = NULL;
    list_ForEach(listEntry, pod_list->items) {
      pod = static_cast<v1_pod_t *>(listEntry->data);
      printf("\tThe pod name: %s\n", pod->metadata->name);
      v1_container_t *container = NULL;
      listEntry_t *containerEntry = NULL;
      list_ForEach(containerEntry, pod->spec->containers) {
        container = static_cast<v1_container_t *>(containerEntry->data);
        printf("\tThe container name: %s\n", container->name);
      }
    }
    v1_pod_list_free(pod_list);
    pod_list = NULL;
  } else {
    printf("Cannot get any pod.\n");
  }
}

void list_namespaces(apiClient_t *apiClient) {
  v1_namespace_list_t *namespace_list = NULL;
  namespace_list = CoreV1API_listNamespace(apiClient,
                                           NULL /*pretty*/,
                                           0 /*allowWatchBookmarks*/,
                                           NULL /*_continue*/,
                                           NULL /*fieldSelector */,
                                           NULL /*labelSelector*/,
                                           0 /*limit*/ ,
                                           NULL /*resourceVersion*/,
                                           NULL /*resourceVersionMatch*/,
                                           0 /*timeoutSeconds*/,
                                           0 /*watch*/);
  printf("The return code of HTTP request=%ld\n", apiClient->response_code);
  if (namespace_list) {
    printf("Get namespaces list:\n");
    listEntry_t *listEntry = NULL;
    v1_namespace_t *ns = NULL;
    list_ForEach(listEntry, namespace_list->items) {
      ns = static_cast<v1_namespace_t *>(listEntry->data);
      printf("\tThe namespace name: %s\n", ns->metadata->name);
    }
    v1_namespace_list_free(namespace_list);
    namespace_list = NULL;
  } else {
    printf("Cannot get any pod.\n");
  }
}

int main() {
  char *basePath = NULL;
  sslConfig_t *sslConfig = NULL;
  list_t *apiKeys = NULL;
  int rc = load_incluster_config(&basePath, &sslConfig, &apiKeys);
  if (rc != 0) {
    printf("Cannot load kubernetes configuration in cluster.\n");
    return -1;
  }
  apiClient_t *apiClient = apiClient_create_with_base_path(basePath, sslConfig, apiKeys);
  if (!apiClient) {
    printf("Cannot create a kubernetes client.\n");
    return -1;
  }
  list_pod(apiClient);
  list_namespaces(apiClient);

  apiClient_free(apiClient);
  apiClient = NULL;
  free_client_config(basePath, sslConfig, apiKeys);
  basePath = NULL;
  sslConfig = NULL;
  apiKeys = NULL;
  apiClient_unsetupGlobalEnv();

  return 0;
}

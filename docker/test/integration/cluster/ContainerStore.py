# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import logging
import shortuuid
from .containers.MinifiContainer import MinifiOptions
from .containers.MinifiContainer import MinifiContainer
from .containers.NifiContainer import NifiContainer
from .containers.NifiContainer import NiFiOptions
from .containers.AzureStorageServerContainer import AzureStorageServerContainer
from .containers.HttpProxyContainer import HttpProxyContainer
from .containers.PostgreSQLServerContainer import PostgreSQLServerContainer
from .containers.SyslogUdpClientContainer import SyslogUdpClientContainer
from .containers.SyslogTcpClientContainer import SyslogTcpClientContainer
from .containers.MinifiAsPodInKubernetesCluster import MinifiAsPodInKubernetesCluster
from .containers.PrometheusContainer import PrometheusContainer
from .FeatureContext import FeatureContext


class ContainerStore:
    def __init__(self, network, image_store, kubernetes_proxy, feature_id):
        self.feature_id = feature_id
        self.minifi_options = MinifiOptions()
        self.containers = {}
        self.data_directories = {}
        self.network = network
        self.image_store = image_store
        self.kubernetes_proxy = kubernetes_proxy
        self.nifi_options = NiFiOptions()

    def get_container_name_with_postfix(self, container_name: str):
        if not container_name.endswith(self.feature_id):
            return container_name + "-" + self.feature_id
        return container_name

    def cleanup(self):
        for container in self.containers.values():
            container.cleanup()
        self.containers = {}
        if self.network:
            logging.info('Cleaning up network: %s', self.network.name)
            self.network.remove()
            self.network = None

    def set_directory_bindings(self, volumes, data_directories):
        self.vols = volumes
        self.data_directories = data_directories
        for container in self.containers.values():
            container.vols = self.vols

    def acquire_container(self, context, container_name: str, engine='minifi-cpp', command=None):
        container_name = self.get_container_name_with_postfix(container_name)
        if container_name is not None and container_name in self.containers:
            return self.containers[container_name]

        if container_name is None and (engine == 'nifi' or engine == 'minifi-cpp'):
            container_name = engine + '-' + shortuuid.uuid()
            logging.info('Container name was not provided; using generated name \'%s\'', container_name)

        feature_context = FeatureContext(feature_id=context.feature_id,
                                         root_ca_cert=context.root_ca_cert,
                                         root_ca_key=context.root_ca_key)

        if engine == 'nifi':
            return self.containers.setdefault(container_name,
                                              NifiContainer(feature_context=feature_context,
                                                            config_dir=self.data_directories["nifi_config_dir"],
                                                            options=self.nifi_options,
                                                            name=container_name,
                                                            vols=self.vols,
                                                            network=self.network,
                                                            image_store=self.image_store,
                                                            command=command))
        elif engine == 'minifi-cpp':
            return self.containers.setdefault(container_name,
                                              MinifiContainer(feature_context=feature_context,
                                                              config_dir=self.data_directories["minifi_config_dir"],
                                                              options=self.minifi_options,
                                                              name=container_name,
                                                              vols=self.vols,
                                                              network=self.network,
                                                              image_store=self.image_store,
                                                              command=command))
        elif engine == 'kubernetes':
            return self.containers.setdefault(container_name,
                                              MinifiAsPodInKubernetesCluster(feature_context=feature_context,
                                                                             kubernetes_proxy=self.kubernetes_proxy,
                                                                             config_dir=self.data_directories["kubernetes_config_dir"],
                                                                             minifi_options=self.minifi_options,
                                                                             name=container_name,
                                                                             vols=self.vols,
                                                                             network=self.network,
                                                                             image_store=self.image_store,
                                                                             command=command))
        elif engine == 'http-proxy':
            return self.containers.setdefault(container_name,
                                              HttpProxyContainer(feature_context=feature_context,
                                                                 name=container_name,
                                                                 vols=self.vols,
                                                                 network=self.network,
                                                                 image_store=self.image_store,
                                                                 command=command))
        elif engine == 'azure-storage-server':
            return self.containers.setdefault(container_name,
                                              AzureStorageServerContainer(feature_context=feature_context,
                                                                          name=container_name,
                                                                          vols=self.vols,
                                                                          network=self.network,
                                                                          image_store=self.image_store,
                                                                          command=command))
        elif engine == 'postgresql-server':
            return self.containers.setdefault(container_name,
                                              PostgreSQLServerContainer(feature_context=feature_context,
                                                                        name=container_name,
                                                                        vols=self.vols,
                                                                        network=self.network,
                                                                        image_store=self.image_store,
                                                                        command=command))
        elif engine == "syslog-udp-client":
            return self.containers.setdefault(container_name,
                                              SyslogUdpClientContainer(
                                                  feature_context=feature_context,
                                                  name=container_name,
                                                  vols=self.vols,
                                                  network=self.network,
                                                  image_store=self.image_store,
                                                  command=command))
        elif engine == "syslog-tcp-client":
            return self.containers.setdefault(container_name,
                                              SyslogTcpClientContainer(
                                                  feature_context=feature_context,
                                                  name=container_name,
                                                  vols=self.vols,
                                                  network=self.network,
                                                  image_store=self.image_store,
                                                  command=command))
        elif engine == "prometheus":
            return self.containers.setdefault(container_name,
                                              PrometheusContainer(feature_context=feature_context,
                                                                  name=container_name,
                                                                  vols=self.vols,
                                                                  network=self.network,
                                                                  image_store=self.image_store,
                                                                  command=command))
        elif engine == "prometheus-ssl":
            return self.containers.setdefault(container_name,
                                              PrometheusContainer(feature_context=feature_context,
                                                                  name=container_name,
                                                                  vols=self.vols,
                                                                  network=self.network,
                                                                  image_store=self.image_store,
                                                                  command=command,
                                                                  ssl=True))
        else:
            raise Exception('invalid flow engine: \'%s\'' % engine)

    def acquire_transient_minifi(self, context, container_name: str, engine='minifi-cpp'):
        self.acquire_container(context=context, container_name=container_name, engine=engine, command=["/bin/sh", "-c", "timeout 10s {run_minifi} && sleep 100".format(run_minifi=MinifiContainer.MINIFI_LOCATIONS.run_minifi_cmd)])

    def deploy_container(self, container_name: str):
        container_name = self.get_container_name_with_postfix(container_name)
        if container_name is None or container_name not in self.containers:
            raise Exception('Invalid container to deploy: \'%s\'' % container_name)

        self.containers[container_name].deploy()

    def deploy_all(self):
        for container in self.containers.values():
            container.deploy()

    def stop_container(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        if container_name not in self.containers:
            logging.error('Could not stop container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].stop()

    def kill_container(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        if container_name not in self.containers:
            logging.error('Could not kill container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].kill()

    def restart_container(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        if container_name not in self.containers:
            logging.error('Could not restart container because it is not found: \'%s\'', container_name)
            return
        self.containers[container_name].restart()

    def enable_provenance_repository_in_minifi(self):
        self.minifi_options.enable_provenance = True

    def set_ssl_context_properties_in_minifi(self):
        self.minifi_options.set_ssl_context_properties = True

    def enable_prometheus_in_minifi(self):
        self.minifi_options.enable_prometheus = True

    def enable_prometheus_with_ssl_in_minifi(self):
        self.minifi_options.enable_prometheus_with_ssl = True

    def enable_sql_in_minifi(self):
        self.minifi_options.enable_sql = True

    def use_nifi_python_processors_with_system_python_packages_installed_in_minifi(self):
        self.minifi_options.use_nifi_python_processors_with_system_python_packages_installed = True

    def use_nifi_python_processors_with_virtualenv_in_minifi(self):
        self.minifi_options.use_nifi_python_processors_with_virtualenv = True

    def use_nifi_python_processors_with_virtualenv_packages_installed_in_minifi(self):
        self.minifi_options.use_nifi_python_processors_with_virtualenv_packages_installed = True

    def remove_python_requirements_txt_in_minifi(self):
        self.minifi_options.remove_python_requirements_txt = True

    def use_nifi_python_processors_without_dependencies_in_minifi(self):
        self.minifi_options.use_nifi_python_processors_without_dependencies = True

    def set_yaml_in_minifi(self):
        self.minifi_options.config_format = "yaml"

    def set_json_in_minifi(self):
        self.minifi_options.config_format = "json"

    def enable_log_metrics_publisher_in_minifi(self):
        self.minifi_options.enable_log_metrics_publisher = True

    def enable_example_minifi_python_processors(self):
        self.minifi_options.enable_example_minifi_python_processors = True

    def enable_openssl_fips_mode_in_minifi(self):
        self.minifi_options.enable_openssl_fips_mode = True

    def disable_openssl_fips_mode_in_minifi(self):
        self.minifi_options.enable_openssl_fips_mode = False

    def llama_model_is_downloaded_in_minifi(self):
        self.minifi_options.download_llama_model = True

    def get_startup_finished_log_entry(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        return self.containers[container_name].get_startup_finished_log_entry()

    def log_source(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        return self.containers[container_name].log_source()

    def get_app_log(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        return self.containers[container_name].get_app_log()

    def get_container_names(self, engine=None):
        return [key for key in self.containers.keys() if not engine or self.containers[key].get_engine() == engine]

    def enable_ssl_in_nifi(self):
        self.nifi_options.use_ssl = True

    def run_post_startup_commands(self, container_name):
        container_name = self.get_container_name_with_postfix(container_name)
        return self.containers[container_name].run_post_startup_commands()

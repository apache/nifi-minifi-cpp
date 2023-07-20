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
from .containers.ZookeeperContainer import ZookeeperContainer
from .containers.KafkaBrokerContainer import KafkaBrokerContainer
from .containers.S3ServerContainer import S3ServerContainer
from .containers.AzureStorageServerContainer import AzureStorageServerContainer
from .containers.FakeGcsServerContainer import FakeGcsServerContainer
from .containers.HttpProxyContainer import HttpProxyContainer
from .containers.PostgreSQLServerContainer import PostgreSQLServerContainer
from .containers.MqttBrokerContainer import MqttBrokerContainer
from .containers.OPCUAServerContainer import OPCUAServerContainer
from .containers.SplunkContainer import SplunkContainer
from .containers.ElasticsearchContainer import ElasticsearchContainer
from .containers.OpensearchContainer import OpensearchContainer
from .containers.SyslogUdpClientContainer import SyslogUdpClientContainer
from .containers.SyslogTcpClientContainer import SyslogTcpClientContainer
from .containers.MinifiAsPodInKubernetesCluster import MinifiAsPodInKubernetesCluster
from .containers.TcpClientContainer import TcpClientContainer
from .containers.PrometheusContainer import PrometheusContainer
from .containers.MinifiC2ServerContainer import MinifiC2ServerContainer
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
                                         root_ca_cert=context.test.root_ca_cert,
                                         root_ca_key=context.test.root_ca_key)

        if engine == 'nifi':
            return self.containers.setdefault(container_name,
                                              NifiContainer(feature_context=feature_context,
                                                            config_dir=self.data_directories["nifi_config_dir"],
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
        elif engine == 'kafka-broker':
            zookeeper_name = self.get_container_name_with_postfix('zookeeper')
            if zookeeper_name not in self.containers:
                self.containers.setdefault(zookeeper_name,
                                           ZookeeperContainer(feature_context=feature_context,
                                                              name=zookeeper_name,
                                                              vols=self.vols,
                                                              network=self.network,
                                                              image_store=self.image_store,
                                                              command=command))
            return self.containers.setdefault(container_name,
                                              KafkaBrokerContainer(feature_context=feature_context,
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
        elif engine == 's3-server':
            return self.containers.setdefault(container_name,
                                              S3ServerContainer(feature_context=feature_context,
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
        elif engine == 'fake-gcs-server':
            return self.containers.setdefault(container_name,
                                              FakeGcsServerContainer(feature_context=feature_context,
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
        elif engine == 'mqtt-broker':
            return self.containers.setdefault(container_name,
                                              MqttBrokerContainer(feature_context=feature_context,
                                                                  name=container_name,
                                                                  vols=self.vols,
                                                                  network=self.network,
                                                                  image_store=self.image_store,
                                                                  command=command))
        elif engine == 'opcua-server':
            return self.containers.setdefault(container_name,
                                              OPCUAServerContainer(feature_context=feature_context,
                                                                   name=container_name,
                                                                   vols=self.vols,
                                                                   network=self.network,
                                                                   image_store=self.image_store,
                                                                   command=command))
        elif engine == 'splunk':
            return self.containers.setdefault(container_name,
                                              SplunkContainer(feature_context=feature_context,
                                                              name=container_name,
                                                              vols=self.vols,
                                                              network=self.network,
                                                              image_store=self.image_store,
                                                              command=command))
        elif engine == 'elasticsearch':
            return self.containers.setdefault(container_name,
                                              ElasticsearchContainer(feature_context=feature_context,
                                                                     name=container_name,
                                                                     vols=self.vols,
                                                                     network=self.network,
                                                                     image_store=self.image_store,
                                                                     command=command))
        elif engine == 'opensearch':
            return self.containers.setdefault(container_name,
                                              OpensearchContainer(feature_context=feature_context,
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
        elif engine == "tcp-client":
            return self.containers.setdefault(container_name,
                                              TcpClientContainer(feature_context=feature_context,
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
        elif engine == "minifi-c2-server":
            return self.containers.setdefault(container_name,
                                              MinifiC2ServerContainer(feature_context=feature_context,
                                                                      name=container_name,
                                                                      vols=self.vols,
                                                                      network=self.network,
                                                                      image_store=self.image_store,
                                                                      command=command,
                                                                      ssl=False))
        elif engine == "minifi-c2-server-ssl":
            return self.containers.setdefault(container_name,
                                              MinifiC2ServerContainer(feature_context=feature_context,
                                                                      name=container_name,
                                                                      vols=self.vols,
                                                                      network=self.network,
                                                                      image_store=self.image_store,
                                                                      command=command,
                                                                      ssl=True))
        else:
            raise Exception('invalid flow engine: \'%s\'' % engine)

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

    def enable_c2_in_minifi(self):
        self.minifi_options.enable_c2 = True

    def enable_c2_with_ssl_in_minifi(self):
        self.minifi_options.enable_c2_with_ssl = True

    def fetch_flow_config_from_c2_url_in_minifi(self):
        self.minifi_options.use_flow_config_from_url = True

    def set_ssl_context_properties_in_minifi(self):
        self.minifi_options.set_ssl_context_properties = True

    def enable_prometheus_in_minifi(self):
        self.minifi_options.enable_prometheus = True

    def enable_prometheus_with_ssl_in_minifi(self):
        self.minifi_options.enable_prometheus_with_ssl = True

    def enable_sql_in_minifi(self):
        self.minifi_options.enable_sql = True

    def set_yaml_in_minifi(self):
        self.minifi_options.config_format = "yaml"

    def set_controller_socket_properties_in_minifi(self):
        self.minifi_options.enable_controller_socket = True

    def enable_log_metrics_publisher_in_minifi(self):
        self.minifi_options.enable_log_metrics_publisher = True

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

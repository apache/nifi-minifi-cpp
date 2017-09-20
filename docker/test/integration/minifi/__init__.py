# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import uuid
import logging

import yaml
import docker


class Cluster(object):
    """
    Base Cluster class. This is intended to be a generic interface
    to different types of clusters. Clusters could be Kubernetes clusters,
    Docker swarms, or cloud compute/container services.
    """

    def deploy_flow(self, flow, name=None, vols=None):
        """
        Deploys a flow to the cluster.
        """

    def __enter__(self):
        """
        Allocate ephemeral cluster resources.
        """
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Clean up ephemeral cluster resources.
        """


class SingleNodeDockerCluster(Cluster):
    """
    A "cluster" which consists of a single docker node. Useful for
    testing or use-cases which do not span multiple compute nodes.
    """

    def __init__(self):
        self.network = None
        self.containers = []
        self.tmp_files = []

        # Get docker client
        self.client = docker.from_env()

    def deploy_flow(self, flow, name=None, vols=None):
        """
        Compiles the flow to YAML and maps it into the container using
        the docker volumes API.
        """

        if vols is None:
            vols = {}

        logging.info('Deploying flow...')

        if name is None:
            name = 'minifi-' + str(uuid.uuid4())
            logging.info('Flow name was not provided; using generated name \'%s\'', name)

        minifi_version = os.environ['MINIFI_VERSION']
        self.minifi_root = '/opt/minifi/nifi-minifi-cpp-' + minifi_version

        # Write flow config
        tmp_flow_file_name = '/tmp/.minifi-flow.' + str(uuid.uuid4())
        self.tmp_files.append(tmp_flow_file_name)

        yaml = flow_yaml(flow)

        logging.info('Using generated flow config yml:\n%s', yaml)

        with open(tmp_flow_file_name, 'w') as tmp_flow_file:
            tmp_flow_file.write(yaml)

        conf_file = tmp_flow_file_name

        local_vols = {}
        local_vols[conf_file] = {'bind': self.minifi_root + '/conf/config.yml', 'mode': 'ro'}
        local_vols.update(vols)

        logging.info('Creating and running docker container for flow...')

        # Create network if necessary
        if self.network is None:
            net_name = 'minifi-' + str(uuid.uuid4())
            logging.info('Creating network: %s', net_name)
            self.network = self.client.networks.create(net_name)

        container = self.client.containers.run(
                'apacheminificpp:' + minifi_version,
                detach=True,
                name=name,
                network=self.network.name,
                volumes=local_vols)

        logging.info('Started container \'%s\'', container.name)
        self.containers.append(container)

    def __enter__(self):
        """
        Allocate ephemeral cluster resources.
        """
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Clean up ephemeral cluster resources
        """

        # Clean up containers
        for container in self.containers:
            logging.info('Cleaning up container: %s', container.name)
            container.remove(v=True, force=True)

        # Clean up network
        if self.network is not None:
            logging.info('Cleaning up network network: %s', self.network.name)
            self.network.remove()

        # Clean up tmp files
        for tmp_file in self.tmp_files:
            os.remove(tmp_file)


class Processor(object):
    def __init__(self,
                 clazz,
                 properties=None,
                 schedule=None,
                 name=None,
                 auto_terminate=None,
                 controller_services=None):

        if controller_services is None:
            controller_services = []

        if auto_terminate is None:
            auto_terminate = []

        if schedule is None:
            schedule = {}

        if properties is None:
            properties = {}

        self.connections = {}
        self.uuid = uuid.uuid4()

        if name is None:
            self.name = str(self.uuid)

        self.clazz = clazz
        self.properties = properties
        self.auto_terminate = auto_terminate
        self.controller_services = controller_services

        self.out_proc = self

        self.schedule = {
            'scheduling strategy': 'EVENT_DRIVEN',
            'scheduling period': '1 sec',
            'penalization period': '30 sec',
            'yield period': '1 sec',
            'run duration nanos': 0
        }
        self.schedule.update(schedule)

    def connect(self, connections):
        for rel in connections:

            # Ensure that rel is not auto-terminated
            if rel in self.auto_terminate:
                del self.auto_terminate[self.auto_terminate.index(rel)]

            # Add to set of output connections for this rel
            if not rel in self.connections:
                self.connections[rel] = []
            self.connections[rel].append(connections[rel])

        return self

    def __rshift__(self, other):
        """
        Right shift operator to support flow DSL, for example:

            GetFile('/input') >> LogAttribute() >> PutFile('/output')

        """

        if isinstance(other, tuple):
            if isinstance(other[0], tuple):
                for rel_tuple in other:
                    rel = {rel_tuple[0]: rel_tuple[1]}
                    self.out_proc.connect(rel)
            else:
                rel = {other[0]: other[1]}
                self.out_proc.connect(rel)
        else:
            self.out_proc.connect({'success': other})
            self.out_proc = other

        return self


def InvokeHTTP(url,
               method='GET',
               ssl_context_service=None):
    properties = {'Remote URL': url, 'HTTP Method': method}

    controller_services = []

    if ssl_context_service is not None:
        properties['SSL Context Service'] = ssl_context_service.name
        controller_services.append(ssl_context_service)

    return Processor('InvokeHTTP',
                     properties=properties,
                     controller_services=controller_services,
                     auto_terminate=['success',
                                     'response',
                                     'retry',
                                     'failure',
                                     'no retry'])


def ListenHTTP(port, cert=None):
    properties = {'Listening Port': port}

    if cert is not None:
        properties['SSL Certificate'] = cert

    return Processor('ListenHTTP',
                     properties=properties,
                     auto_terminate=['success'])


def LogAttribute():
    return Processor('LogAttribute',
                     auto_terminate=['success'])


def GetFile(input_dir):
    return Processor('GetFile',
                     properties={'Input Directory': input_dir},
                     schedule={'scheduling period': '0 sec'},
                     auto_terminate=['success'])


def PutFile(output_dir):
    return Processor('PutFile',
                     properties={'Output Directory': output_dir},
                     auto_terminate=['success', 'failure'])


class ControllerService(object):
    def __init__(self, name=None, properties=None):

        self.id = str(uuid.uuid4())

        if name is None:
            self.name = str(uuid.uuid4())
            logging.info('Controller service name was not provided; using generated name \'%s\'', self.name)
        else:
            self.name = name

        if properties is None:
            properties = {}

        self.properties = properties


class SSLContextService(ControllerService):
    def __init__(self, name=None, cert=None, key=None, ca_cert=None):
        super(SSLContextService, self).__init__(name=name)

        self.service_class = 'SSLContextService'

        if cert is not None:
            self.properties['Client Certificate'] = cert

        if key is not None:
            self.properties['Private Key'] = key

        if ca_cert is not None:
            self.properties['CA Certificate'] = ca_cert


def flow_yaml(processor, root=None, visited=None):
    if visited is None:
        visited = []

    if root is None:
        res = {
            'Flow Controller': {
                'name': 'MiNiFi Flow'
            },
            'Processors': [],
            'Connections': [],
            'Remote Processing Groups': [],
            'Controller Services': []
        }
    else:
        res = root

    visited.append(processor)

    if hasattr(processor, 'name'):
        proc_name = processor.name
    else:
        proc_name = str(processor.uuid)

    res['Processors'].append({
        'name': proc_name,
        'id': str(processor.uuid),
        'class': 'org.apache.nifi.processors.standard.' + processor.clazz,
        'scheduling strategy': processor.schedule['scheduling strategy'],
        'scheduling period': processor.schedule['scheduling period'],
        'penalization period': processor.schedule['penalization period'],
        'yield period': processor.schedule['yield period'],
        'run duration nanos': processor.schedule['run duration nanos'],
        'Properties': processor.properties,
        'auto-terminated relationships list': processor.auto_terminate
    })

    for svc in processor.controller_services:
        if svc in visited:
            continue

        visited.append(svc)
        res['Controller Services'].append({
            'name': svc.name,
            'id': svc.id,
            'class': svc.service_class,
            'Properties': svc.properties
        })

    for conn_name in processor.connections:
        conn_procs = processor.connections[conn_name]

        if isinstance(conn_procs, list):
            for proc in conn_procs:
                res['Connections'].append({
                    'name': str(uuid.uuid4()),
                    'source id': str(processor.uuid),
                    'source relationship name': conn_name,
                    'destination id': str(proc.uuid)
                })
                if proc not in visited:
                    flow_yaml(proc, res, visited)
        else:
            res['Connections'].append({
                'name': str(uuid.uuid4()),
                'source id': str(processor.uuid),
                'source relationship name': conn_name,
                'destination id': str(conn_procs.uuid)
            })
            if conn_procs not in visited:
                flow_yaml(conn_procs, res, visited)

    if root is None:
        return yaml.dump(res, default_flow_style=False)

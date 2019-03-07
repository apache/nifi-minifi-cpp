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
import gzip
import logging
import tarfile
import uuid
import xml.etree.cElementTree as elementTree
from xml.etree.cElementTree import Element
from io import StringIO
from io import BytesIO
from textwrap import dedent

import docker
import os
import yaml
from copy import copy


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
        self.minifi_version = os.environ['MINIFI_VERSION']
        self.nifi_version = '1.7.0'
        self.minifi_root = '/opt/minifi/nifi-minifi-cpp-' + self.minifi_version
        self.nifi_root = '/opt/nifi/nifi-' + self.nifi_version
        self.network = None
        self.containers = []
        self.images = []
        self.tmp_files = []

        # Get docker client
        self.client = docker.from_env()

    def deploy_flow(self,
                    flow,
                    name=None,
                    vols=None,
                    engine='minifi-cpp'):
        """
        Compiles the flow to a valid config file and overlays it into a new image.
        """

        if vols is None:
            vols = {}

        logging.info('Deploying %s flow...%s', engine,name)

        if name is None:
            name = engine + '-' + str(uuid.uuid4())
            logging.info('Flow name was not provided; using generated name \'%s\'', name)

        # Create network if necessary
        if self.network is None:
            net_name = 'nifi-' + str(uuid.uuid4())
            logging.info('Creating network: %s', net_name)
            self.network = self.client.networks.create(net_name)

        if engine == 'nifi':
            self.deploy_nifi_flow(flow, name, vols)
        elif engine == 'minifi-cpp':
            self.deploy_minifi_cpp_flow(flow, name, vols)
        else:
            raise Exception('invalid flow engine: \'%s\'' % engine)

    def deploy_minifi_cpp_flow(self, flow, name, vols):

        # Build configured image
        dockerfile = dedent("""FROM {base_image}
                USER root
                ADD config.yml {minifi_root}/conf/config.yml
                RUN chown minificpp:minificpp {minifi_root}/conf/config.yml
                USER minificpp
                """.format(name=name,hostname=name,
                           base_image='apacheminificpp:' + self.minifi_version,
                           minifi_root=self.minifi_root))

        test_flow_yaml = minifi_flow_yaml(flow)
        logging.info('Using generated flow config yml:\n%s', test_flow_yaml)

        conf_file_buffer = BytesIO()

        try:
            conf_file_buffer.write(test_flow_yaml.encode('utf-8'))
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'config.yml',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        logging.info('Creating and running docker container for flow...')

        container = self.client.containers.run(
                configured_image[0],
                detach=True,
                name=name,
                network=self.network.name,
                volumes=vols)

        logging.info('Started container \'%s\'', container.name)

        self.containers.append(container)

    def deploy_nifi_flow(self, flow, name, vols):
        dockerfile = dedent("""FROM {base_image}
                USER root
                ADD flow.xml.gz {nifi_root}/conf/flow.xml.gz
                RUN chown nifi:nifi {nifi_root}/conf/flow.xml.gz
                RUN sed -i -e 's/^\(nifi.remote.input.host\)=.*/\\1={name}/' {nifi_root}/conf/nifi.properties
                RUN sed -i -e 's/^\(nifi.remote.input.socket.port\)=.*/\\1=5000/' {nifi_root}/conf/nifi.properties
                USER nifi
                """.format(name=name,
                           base_image='apache/nifi:' + self.nifi_version,
                           nifi_root=self.nifi_root))

        test_flow_xml = nifi_flow_xml(flow, self.nifi_version)
        logging.info('Using generated flow config xml:\n%s', test_flow_xml)

        conf_file_buffer = BytesIO()

        try:
            with gzip.GzipFile(mode='wb', fileobj=conf_file_buffer) as conf_gz_file_buffer:
                conf_gz_file_buffer.write(test_flow_xml.encode())
            conf_file_len = conf_file_buffer.tell()
            conf_file_buffer.seek(0)

            context_files = [
                {
                    'name': 'flow.xml.gz',
                    'size': conf_file_len,
                    'file_obj': conf_file_buffer
                }
            ]

            configured_image = self.build_image(dockerfile, context_files)

        finally:
            conf_file_buffer.close()

        logging.info('Creating and running docker container for flow...')

        container = self.client.containers.run(
                configured_image[0],
                detach=True,
                name=name,
                hostname=name,
                network=self.network.name,
                volumes=vols)

        logging.info('Started container \'%s\'', container.name)

        self.containers.append(container)

    def build_image(self, dockerfile, context_files):
        conf_dockerfile_buffer = BytesIO()
        docker_context_buffer = BytesIO()

        try:
            # Overlay conf onto base nifi image
            conf_dockerfile_buffer.write(dockerfile.encode())
            conf_dockerfile_buffer.seek(0)

            with tarfile.open(mode='w', fileobj=docker_context_buffer) as docker_context:
                dockerfile_info = tarfile.TarInfo('Dockerfile')
                dockerfile_info.size = len(conf_dockerfile_buffer.getvalue())
                docker_context.addfile(dockerfile_info,
                                       fileobj=conf_dockerfile_buffer)

                for context_file in context_files:
                    file_info = tarfile.TarInfo(context_file['name'])
                    file_info.size = context_file['size']
                    docker_context.addfile(file_info,
                                           fileobj=context_file['file_obj'])
            docker_context_buffer.seek(0)

            logging.info('Creating configured image...')
            configured_image = self.client.images.build(fileobj=docker_context_buffer,
                                                        custom_context=True,
                                                        rm=True,
                                                        forcerm=True)
            self.images.append(configured_image)

        finally:
            conf_dockerfile_buffer.close()
            docker_context_buffer.close()

        return configured_image

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

        # Clean up images
        for image in self.images:
            logging.info('Cleaning up image: %s', image[0].id)
            self.client.images.remove(image[0].id, force=True)

        # Clean up network
        if self.network is not None:
            logging.info('Cleaning up network network: %s', self.network.name)
            self.network.remove()

        # Clean up tmp files
        for tmp_file in self.tmp_files:
            os.remove(tmp_file)


class Connectable(object):
    def __init__(self,
                 name=None,
                 auto_terminate=None):

        self.uuid = uuid.uuid4()

        if name is None:
            self.name = str(self.uuid)
        else:
            self.name = name

        if auto_terminate is None:
            self.auto_terminate = []
        else:
            self.auto_terminate = auto_terminate

        self.connections = {}
        self.out_proc = self

    def connect(self, connections):
        for rel in connections:

            # Ensure that rel is not auto-terminated
            if rel in self.auto_terminate:
                del self.auto_terminate[self.auto_terminate.index(rel)]

            # Add to set of output connections for this rel
            if rel not in self.connections:
                self.connections[rel] = []
            self.connections[rel].append(connections[rel])

        return self

    def __rshift__(self, other):
        """
        Right shift operator to support flow DSL, for example:

            GetFile('/input') >> LogAttribute() >> PutFile('/output')

        """

        connected = copy(self)
        connected.connections = copy(self.connections)

        if self.out_proc is self:
            connected.out_proc = connected
        else:
            connected.out_proc = copy(connected.out_proc)

        if isinstance(other, tuple):
            if isinstance(other[0], tuple):
                for rel_tuple in other:
                    rel = {rel_tuple[0]: rel_tuple[1]}
                    connected.out_proc.connect(rel)
            else:
                rel = {other[0]: other[1]}
                connected.out_proc.connect(rel)
        else:
            connected.out_proc.connect({'success': other})
            connected.out_proc = other

        return connected


class Processor(Connectable):
    def __init__(self,
                 clazz,
                 properties=None,
                 schedule=None,
                 name=None,
                 controller_services=None,
                 auto_terminate=None):

        super(Processor, self).__init__(name=name,
                                        auto_terminate=auto_terminate)

        if controller_services is None:
            controller_services = []

        if schedule is None:
            schedule = {}

        if properties is None:
            properties = {}

        if name is None:
            pass

        self.clazz = clazz
        self.properties = properties
        self.controller_services = controller_services

        self.schedule = {
            'scheduling strategy': 'EVENT_DRIVEN',
            'scheduling period': '1 sec',
            'penalization period': '30 sec',
            'yield period': '1 sec',
            'run duration nanos': 0
        }
        self.schedule.update(schedule)

    def nifi_property_key(self, key):
        """
        Returns the Apache NiFi-equivalent property key for the given key. This is often, but not always, the same as
        the internal key.
        """
        return key


class InvokeHTTP(Processor):
    def __init__(self, url,
                 method='GET',
                 ssl_context_service=None):
        properties = {'Remote URL': url, 'HTTP Method': method}

        controller_services = []

        if ssl_context_service is not None:
            properties['SSL Context Service'] = ssl_context_service.name
            controller_services.append(ssl_context_service)

        super(InvokeHTTP, self).__init__('InvokeHTTP',
                                         properties=properties,
                                         controller_services=controller_services,
                                         auto_terminate=['success',
                                                         'response',
                                                         'retry',
                                                         'failure',
                                                         'no retry'])


class ListenHTTP(Processor):
    def __init__(self, port, cert=None):
        properties = {'Listening Port': port}

        if cert is not None:
            properties['SSL Certificate'] = cert
            properties['SSL Verify Peer'] = 'no'

        super(ListenHTTP, self).__init__('ListenHTTP',
                                         properties=properties,
                                         auto_terminate=['success'])


class LogAttribute(Processor):
    def __init__(self, ):
        super(LogAttribute, self).__init__('LogAttribute',
                                           auto_terminate=['success'])
        
        
class DebugFlow(Processor):
    def __init__(self, ):
        super(DebugFlow, self).__init__('DebugFlow')

class HashAttribute(Processor):
    def __init__(self, attributename):
        super(HashAttribute, self).__init__('HashAttribute',
                                           properties={'Hash Value Attribute Key': attributename},
                                           auto_terminate=['failure'])

class AttributesToJSON(Processor):
    def __init__(self, destination, attributes):
        super(AttributesToJSON, self).__init__('AttributesToJSON',
                                      properties={'Destination': destination, 'Attributes List': attributes},
                                      schedule={'scheduling period': '0 sec'},
                                      auto_terminate=['failure'])
        

class GetFile(Processor):
    def __init__(self, input_dir):
        super(GetFile, self).__init__('GetFile',
                                      properties={'Input Directory': input_dir, 'Keep Source File': 'true'},
                                      schedule={'scheduling period': '2 sec'},
                                      auto_terminate=['success'])

class GenerateFlowFile(Processor):
    def __init__(self, file_size):
        super(GenerateFlowFile, self).__init__('GenerateFlowFile',
                                      properties={'File Size': file_size},
                                      schedule={'scheduling period': '0 sec'},
                                      auto_terminate=['success'])



class PutFile(Processor):
    def __init__(self, output_dir):
        super(PutFile, self).__init__('PutFile',
                                      properties={'Directory': output_dir},
                                      auto_terminate=['success', 'failure'])

    def nifi_property_key(self, key):
        if key == 'Output Directory':
            return 'Directory'
        else:
            return key


class InputPort(Connectable):
    def __init__(self, name=None, remote_process_group=None):
        super(InputPort, self).__init__(name=name)

        self.remote_process_group = remote_process_group


class RemoteProcessGroup(object):
    def __init__(self, url,
                 name=None):

        self.uuid = uuid.uuid4()

        if name is None:
            self.name = str(self.uuid)
        else:
            self.name = name

        self.url = url


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


def minifi_flow_yaml(connectable, root=None, visited=None):
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

    visited.append(connectable)

    if hasattr(connectable, 'name'):
        connectable_name = connectable.name
    else:
        connectable_name = str(connectable.uuid)

    if isinstance(connectable, InputPort):
        group = connectable.remote_process_group
        res_group = None

        for res_group_candidate in res['Remote Processing Groups']:
            assert isinstance(res_group_candidate, dict)
            if res_group_candidate['id'] == str(group.uuid):
                res_group = res_group_candidate

        if res_group is None:
            res_group = {
                'name': group.name,
                'id': str(group.uuid),
                'url': group.url,
                'timeout': '30 sec',
                'yield period': '10 sec',
                'Input Ports': []
            }

            res['Remote Processing Groups'].append(res_group)

        res_group['Input Ports'].append({
            'id': str(connectable.uuid),
            'name': connectable.name,
            'max concurrent tasks': 1,
            'Properties': {}
        })

    if isinstance(connectable, Processor):
        res['Processors'].append({
            'name': connectable_name,
            'id': str(connectable.uuid),
            'class': 'org.apache.nifi.processors.standard.' + connectable.clazz,
            'scheduling strategy': connectable.schedule['scheduling strategy'],
            'scheduling period': connectable.schedule['scheduling period'],
            'penalization period': connectable.schedule['penalization period'],
            'yield period': connectable.schedule['yield period'],
            'run duration nanos': connectable.schedule['run duration nanos'],
            'Properties': connectable.properties,
            'auto-terminated relationships list': connectable.auto_terminate
        })

        for svc in connectable.controller_services:
            if svc in visited:
                continue

            visited.append(svc)
            res['Controller Services'].append({
                'name': svc.name,
                'id': svc.id,
                'class': svc.service_class,
                'Properties': svc.properties
            })

    for conn_name in connectable.connections:
        conn_procs = connectable.connections[conn_name]

        if isinstance(conn_procs, list):
            for proc in conn_procs:
                res['Connections'].append({
                    'name': str(uuid.uuid4()),
                    'source id': str(connectable.uuid),
                    'source relationship name': conn_name,
                    'destination id': str(proc.uuid)
                })
                if proc not in visited:
                    minifi_flow_yaml(proc, res, visited)
        else:
            res['Connections'].append({
                'name': str(uuid.uuid4()),
                'source id': str(connectable.uuid),
                'source relationship name': conn_name,
                'destination id': str(conn_procs.uuid)
            })
            if conn_procs not in visited:
                minifi_flow_yaml(conn_procs, res, visited)

    if root is None:
        return yaml.dump(res, default_flow_style=False)


def nifi_flow_xml(connectable, nifi_version=None, root=None, visited=None):
    if visited is None:
        visited = []

    position = Element('position')
    position.set('x', '0.0')
    position.set('y', '0.0')

    comment = Element('comment')
    styles = Element('styles')
    bend_points = Element('bendPoints')
    label_index = Element('labelIndex')
    label_index.text = '1'
    z_index = Element('zIndex')
    z_index.text = '0'

    if root is None:
        res = Element('flowController')
        max_timer_driven_thread_count = Element('maxTimerDrivenThreadCount')
        max_timer_driven_thread_count.text = '10'
        res.append(max_timer_driven_thread_count)
        max_event_driven_thread_count = Element('maxEventDrivenThreadCount')
        max_event_driven_thread_count.text = '5'
        res.append(max_event_driven_thread_count)
        root_group = Element('rootGroup')
        root_group_id = Element('id')
        root_group_id_text = str(uuid.uuid4())
        root_group_id.text = root_group_id_text
        root_group.append(root_group_id)
        root_group_name = Element('name')
        root_group_name.text = root_group_id_text
        root_group.append(root_group_name)
        res.append(root_group)
        root_group.append(position)
        root_group.append(comment)
        res.append(Element('controllerServices'))
        res.append(Element('reportingTasks'))
        res.set('encoding-version', '1.2')
    else:
        res = root

    visited.append(connectable)

    if hasattr(connectable, 'name'):
        connectable_name_text = connectable.name
    else:
        connectable_name_text = str(connectable.uuid)

    if isinstance(connectable, InputPort):
        input_port = Element('inputPort')

        input_port_id = Element('id')
        input_port_id.text = str(connectable.uuid)
        input_port.append(input_port_id)

        input_port_name = Element('name')
        input_port_name.text = connectable_name_text
        input_port.append(input_port_name)

        input_port.append(position)
        input_port.append(comment)

        input_port_scheduled_state = Element('scheduledState')
        input_port_scheduled_state.text = 'RUNNING'
        input_port.append(input_port_scheduled_state)

        input_port_max_concurrent_tasks = Element('maxConcurrentTasks')
        input_port_max_concurrent_tasks.text = '1'
        input_port.append(input_port_max_concurrent_tasks)
        next( res.iterfind('rootGroup') ).append(input_port)

    if isinstance(connectable, Processor):
        conn_destination = Element('processor')

        proc_id = Element('id')
        proc_id.text = str(connectable.uuid)
        conn_destination.append(proc_id)

        proc_name = Element('name')
        proc_name.text = connectable_name_text
        conn_destination.append(proc_name)

        conn_destination.append(position)
        conn_destination.append(styles)
        conn_destination.append(comment)

        proc_class = Element('class')
        proc_class.text = 'org.apache.nifi.processors.standard.' + connectable.clazz
        conn_destination.append(proc_class)

        proc_bundle = Element('bundle')
        proc_bundle_group = Element('group')
        proc_bundle_group.text = 'org.apache.nifi'
        proc_bundle.append(proc_bundle_group)
        proc_bundle_artifact = Element('artifact')
        proc_bundle_artifact.text = 'nifi-standard-nar'
        proc_bundle.append(proc_bundle_artifact)
        proc_bundle_version = Element('version')
        proc_bundle_version.text = nifi_version
        proc_bundle.append(proc_bundle_version)
        conn_destination.append(proc_bundle)

        proc_max_concurrent_tasks = Element('maxConcurrentTasks')
        proc_max_concurrent_tasks.text = '1'
        conn_destination.append(proc_max_concurrent_tasks)

        proc_scheduling_period = Element('schedulingPeriod')
        proc_scheduling_period.text = connectable.schedule['scheduling period']
        conn_destination.append(proc_scheduling_period)

        proc_penalization_period = Element('penalizationPeriod')
        proc_penalization_period.text = connectable.schedule['penalization period']
        conn_destination.append(proc_penalization_period)

        proc_yield_period = Element('yieldPeriod')
        proc_yield_period.text = connectable.schedule['yield period']
        conn_destination.append(proc_yield_period)

        proc_bulletin_level = Element('bulletinLevel')
        proc_bulletin_level.text = 'WARN'
        conn_destination.append(proc_bulletin_level)

        proc_loss_tolerant = Element('lossTolerant')
        proc_loss_tolerant.text = 'false'
        conn_destination.append(proc_loss_tolerant)

        proc_scheduled_state = Element('scheduledState')
        proc_scheduled_state.text = 'RUNNING'
        conn_destination.append(proc_scheduled_state)

        proc_scheduling_strategy = Element('schedulingStrategy')
        proc_scheduling_strategy.text = connectable.schedule['scheduling strategy']
        conn_destination.append(proc_scheduling_strategy)

        proc_execution_node = Element('executionNode')
        proc_execution_node.text = 'ALL'
        conn_destination.append(proc_execution_node)

        proc_run_duration_nanos = Element('runDurationNanos')
        proc_run_duration_nanos.text = str(connectable.schedule['run duration nanos'])
        conn_destination.append(proc_run_duration_nanos)

        for property_key, property_value in connectable.properties.items():
            proc_property = Element('property')
            proc_property_name = Element('name')
            proc_property_name.text = connectable.nifi_property_key(property_key)
            proc_property.append(proc_property_name)
            proc_property_value = Element('value')
            proc_property_value.text = property_value
            proc_property.append(proc_property_value)
            conn_destination.append(proc_property)

        for auto_terminate_rel in connectable.auto_terminate:
            proc_auto_terminated_relationship = Element('autoTerminatedRelationship')
            proc_auto_terminated_relationship.text = auto_terminate_rel
            conn_destination.append(proc_auto_terminated_relationship)
        next( res.iterfind('rootGroup') ).append(conn_destination)
        """ res.iterfind('rootGroup').next().append(conn_destination) """

        for svc in connectable.controller_services:
            if svc in visited:
                continue

            visited.append(svc)
            controller_service = Element('controllerService')

            controller_service_id = Element('id')
            controller_service_id.text = str(svc.id)
            controller_service.append(controller_service_id)

            controller_service_name = Element('name')
            controller_service_name.text = svc.name
            controller_service.append(controller_service_name)

            controller_service.append(comment)

            controller_service_class = Element('class')
            controller_service_class.text = svc.service_class,
            controller_service.append(controller_service_class)

            controller_service_bundle = Element('bundle')
            controller_service_bundle_group = Element('group')
            controller_service_bundle_group.text = svc.group
            controller_service_bundle.append(controller_service_bundle_group)
            controller_service_bundle_artifact = Element('artifact')
            controller_service_bundle_artifact.text = svc.artifact
            controller_service_bundle.append(controller_service_bundle_artifact)
            controller_service_bundle_version = Element('version')
            controller_service_bundle_version.text = nifi_version
            controller_service_bundle.append(controller_service_bundle_version)
            controller_service.append(controller_service_bundle)

            controller_enabled = Element('enabled')
            controller_enabled.text = 'true',
            controller_service.append(controller_enabled)

            for property_key, property_value in svc.properties:
                controller_service_property = Element('property')
                controller_service_property_name = Element('name')
                controller_service_property_name.text = property_key
                controller_service_property.append(controller_service_property_name)
                controller_service_property_value = Element('value')
                controller_service_property_value.text = property_value
                controller_service_property.append(controller_service_property_value)
                controller_service.append(controller_service_property)
            next( res.iterfind('rootGroup') ).append(controller_service)
            """ res.iterfind('rootGroup').next().append(controller_service)"""

    for conn_name in connectable.connections:
        conn_destinations = connectable.connections[conn_name]

        if isinstance(conn_destinations, list):
            for conn_destination in conn_destinations:
                connection = nifi_flow_xml_connection(res,
                                                      bend_points,
                                                      conn_name,
                                                      connectable,
                                                      label_index,
                                                      conn_destination,
                                                      z_index)
                next( res.iterfind('rootGroup') ).append(connection)
                """ res.iterfind('rootGroup').next().append(connection) """

                if conn_destination not in visited:
                    nifi_flow_xml(conn_destination, nifi_version, res, visited)
        else:
            connection = nifi_flow_xml_connection(res,
                                                  bend_points,
                                                  conn_name,
                                                  connectable,
                                                  label_index,
                                                  conn_destinations,
                                                  z_index)
            next( res.iterfind('rootGroup') ).append(connection)
            """ res.iterfind('rootGroup').next().append(connection) """

            if conn_destinations not in visited:
                nifi_flow_xml(conn_destinations, nifi_version, res, visited)

    if root is None:
        return ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>'
                + "\n"
                + elementTree.tostring(res, encoding='utf-8').decode('utf-8'))


def nifi_flow_xml_connection(res, bend_points, conn_name, connectable, label_index, destination, z_index):
    connection = Element('connection')

    connection_id = Element('id')
    connection_id.text = str(uuid.uuid4())
    connection.append(connection_id)

    connection_name = Element('name')
    connection.append(connection_name)

    connection.append(bend_points)
    connection.append(label_index)
    connection.append(z_index)

    connection_source_id = Element('sourceId')
    connection_source_id.text = str(connectable.uuid)
    connection.append(connection_source_id)

    connection_source_group_id = Element('sourceGroupId')
    connection_source_group_id.text = next( res.iterfind('rootGroup/id') ).text
    """connection_source_group_id.text = res.iterfind('rootGroup/id').next().text"""
    connection.append(connection_source_group_id)

    connection_source_type = Element('sourceType')
    if isinstance(connectable, Processor):
        connection_source_type.text = 'PROCESSOR'
    elif isinstance(connectable, InputPort):
        connection_source_type.text = 'INPUT_PORT'
    else:
        raise Exception('Unexpected source type: %s' % type(connectable))
    connection.append(connection_source_type)

    connection_destination_id = Element('destinationId')
    connection_destination_id.text = str(destination.uuid)
    connection.append(connection_destination_id)

    connection_destination_group_id = Element('destinationGroupId')
    connection_destination_group_id.text = next(res.iterfind('rootGroup/id')).text
    """ connection_destination_group_id.text = res.iterfind('rootGroup/id').next().text """
    connection.append(connection_destination_group_id)

    connection_destination_type = Element('destinationType')
    if isinstance(destination, Processor):
        connection_destination_type.text = 'PROCESSOR'
    elif isinstance(destination, InputPort):
        connection_destination_type.text = 'INPUT_PORT'
    else:
        raise Exception('Unexpected destination type: %s' % type(destination))
    connection.append(connection_destination_type)

    connection_relationship = Element('relationship')
    if not isinstance(connectable, InputPort):
        connection_relationship.text = conn_name
    connection.append(connection_relationship)

    connection_max_work_queue_size = Element('maxWorkQueueSize')
    connection_max_work_queue_size.text = '10000'
    connection.append(connection_max_work_queue_size)

    connection_max_work_queue_data_size = Element('maxWorkQueueDataSize')
    connection_max_work_queue_data_size.text = '1 GB'
    connection.append(connection_max_work_queue_data_size)

    connection_flow_file_expiration = Element('flowFileExpiration')
    connection_flow_file_expiration.text = '0 sec'
    connection.append(connection_flow_file_expiration)

    return connection

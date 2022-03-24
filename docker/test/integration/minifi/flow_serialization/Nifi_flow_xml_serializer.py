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


import uuid

import xml.etree.cElementTree as elementTree
from xml.etree.cElementTree import Element

from ..core.Processor import Processor
from ..core.InputPort import InputPort


class Nifi_flow_xml_serializer:
    def serialize(self, start_nodes, nifi_version=None):
        res = None
        visited = None

        for node in start_nodes:
            res, visited = self.serialize_node(node, nifi_version, res, visited)

        return ('<?xml version="1.0" encoding="UTF-8" standalone="no"?>' + "\n" + elementTree.tostring(res, encoding='utf-8').decode('utf-8'))

    def serialize_node(self, connectable, nifi_version=None, root=None, visited=None):
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
            next(res.iterfind('rootGroup')).append(input_port)

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
            proc_max_concurrent_tasks.text = str(connectable.max_concurrent_tasks)
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
                if not proc_property_name.text:
                    continue
                proc_property.append(proc_property_name)
                proc_property_value = Element('value')
                proc_property_value.text = str(property_value)
                proc_property.append(proc_property_value)
                conn_destination.append(proc_property)

            for auto_terminate_rel in connectable.auto_terminate:
                proc_auto_terminated_relationship = Element('autoTerminatedRelationship')
                proc_auto_terminated_relationship.text = auto_terminate_rel
                conn_destination.append(proc_auto_terminated_relationship)
            next(res.iterfind('rootGroup')).append(conn_destination)
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
                next(res.iterfind('rootGroup')).append(controller_service)
                """ res.iterfind('rootGroup').next().append(controller_service)"""

        for conn_name in connectable.connections:
            conn_destinations = connectable.connections[conn_name]

            if isinstance(conn_destinations, list):
                for conn_destination in conn_destinations:
                    connection = self.build_nifi_flow_xml_connection_element(
                        res,
                        bend_points,
                        conn_name,
                        connectable,
                        label_index,
                        conn_destination,
                        z_index)
                    next(res.iterfind('rootGroup')).append(connection)
                    """ res.iterfind('rootGroup').next().append(connection) """

                    if conn_destination not in visited:
                        self.serialize_node(conn_destination, nifi_version, res, visited)
            else:
                connection = self.build_nifi_flow_xml_connection_element(
                    res,
                    bend_points,
                    conn_name,
                    connectable,
                    label_index,
                    conn_destinations,
                    z_index)
                next(res.iterfind('rootGroup')).append(connection)
                """ res.iterfind('rootGroup').next().append(connection) """

                if conn_destinations not in visited:
                    self.serialize_node(conn_destinations, nifi_version, res, visited)

        return (res, visited)

    def build_nifi_flow_xml_connection_element(self, res, bend_points, conn_name, connectable, label_index, destination, z_index):
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
        connection_source_group_id.text = next(res.iterfind('rootGroup/id')).text
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

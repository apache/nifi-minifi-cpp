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

import traceback
import json
from abc import abstractmethod
from minifi_native import ProcessContext, ProcessSession, Processor
from .processorbase import ProcessorBase
from .properties import FlowFile as FlowFileProxy
from .properties import ProcessContext as ProcessContextProxy
from .properties import PropertyDescriptor


class __RecordTransformResult__:
    def __init__(self, processor_result, recordJson):
        self.processor_result = processor_result
        self.recordJson = recordJson

    def getRecordJson(self):
        return self.recordJson

    def getSchema(self):
        return self.processor_result.schema

    def getRelationship(self):
        return self.processor_result.relationship

    def getPartition(self):
        return self.processor_result.partition


class RecordTransformResult:
    def __init__(self, record=None, schema=None, relationship="success", partition=None):
        self.record = record
        self.schema = schema
        self.relationship = relationship
        self.partition = partition

    def getRecord(self):
        return self.record

    def getSchema(self):
        return self.schema

    def getRelationship(self):
        return self.relationship

    def getPartition(self):
        return self.partition


class RecordTransform(ProcessorBase):
    RECORD_READER = PropertyDescriptor(
        name='Record Reader',
        display_name='Record Reader',
        description='''Specifies the Controller Service to use for reading incoming data''',
        required=True,
        controller_service_definition='RecordSetReader'
    )
    RECORD_WRITER = PropertyDescriptor(
        name='Record Writer',
        display_name='Record Writer',
        description='''Specifies the Controller Service to use for writing out the records''',
        required=True,
        controller_service_definition='RecordSetWriter',
    )

    def onInitialize(self, processor: Processor):
        super(RecordTransform, self).onInitialize(processor)
        processor.addProperty(self.RECORD_READER.name, self.RECORD_READER.description, None, self.RECORD_READER.required, False, False, None, None, self.RECORD_READER.controllerServiceDefinition)
        processor.addProperty(self.RECORD_WRITER.name, self.RECORD_WRITER.description, None, self.RECORD_WRITER.required, False, False, None, None, self.RECORD_WRITER.controllerServiceDefinition)

    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        flow_file = session.get()
        if not flow_file:
            return

        context_proxy = ProcessContextProxy(context, self)
        record_reader = context_proxy.getProperty(self.RECORD_READER).asControllerService()
        if not record_reader:
            self.logger.error("Record Reader property is invalid")
            session.transfer(flow_file, self.REL_FAILURE)
            return
        record_writer = context_proxy.getProperty(self.RECORD_WRITER).asControllerService()
        if not record_writer:
            self.logger.error("Record Writer property is invalid")
            session.transfer(flow_file, self.REL_FAILURE)
            return

        try:
            record_list = record_reader.read(flow_file, session)
            if record_list is None:
                self.logger.error("Reading flow file records returned None")
                session.transfer(flow_file, self.REL_FAILURE)
                return
        except Exception:
            self.logger.error("Failed to read flow file records due to the following error:\n{}".format(traceback.format_exc()))
            session.transfer(flow_file, self.REL_FAILURE)
            return

        flow_file_proxy = FlowFileProxy(session, flow_file)
        results = []
        for record in record_list:
            record_json = json.loads(record)
            try:
                result = self.transform(context_proxy, record_json, None, flow_file_proxy)
                result_record = result.getRecord()
                resultjson = None if result_record is None else json.dumps(result_record)
                results.append(__RecordTransformResult__(result, resultjson))
            except Exception:
                self.logger.error("Failed to transform record due to the following error:\n{}".format(traceback.format_exc()))
                session.transfer(flow_file, self.REL_FAILURE)
                return

        partitions = []
        partitioned_results_list = []
        for result in results:
            if result.getRecordJson() is None:
                continue
            record_partition = result.getPartition()
            try:
                partition_index = partitions.index(record_partition)
                partitioned_results_list[partition_index].append(result)
            except ValueError:
                partitions.append(record_partition)
                partitioned_results_list.append([result])

        for single_partition_results in partitioned_results_list:
            partitioned_flow_file = session.create(flow_file)
            record_writer.write([result.getRecordJson() for result in single_partition_results], partitioned_flow_file, session)
            if result.getRelationship() == "success":
                session.transfer(partitioned_flow_file, self.REL_SUCCESS)
            else:
                session.transferToCustomRelationship(partitioned_flow_file, result.getRelationship())

        session.transfer(flow_file, self.REL_ORIGINAL)

    @abstractmethod
    def transform(self, context: ProcessContextProxy, flowFile: FlowFileProxy) -> RecordTransformResult:
        pass

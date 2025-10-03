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

from abc import ABC, abstractmethod
from typing import List
from .properties import ExpressionLanguageScope, PropertyDescriptor, translateStandardValidatorToMiNiFiPropertype, MinifiPropertyTypes
from .properties import ProcessContext as ProcessContextProxy
from minifi_native import OutputStream, Processor, ProcessContext, ProcessSession


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream: OutputStream):
        output_stream.write(self.content)
        return len(self.content)


class ProcessorBase(ABC):
    # These will be added through the python bindings using C API
    logger = None
    REL_SUCCESS = None
    REL_FAILURE = None
    REL_ORIGINAL = None

    def describe(self, processor: Processor):
        if hasattr(self, 'ProcessorDetails') and hasattr(self.ProcessorDetails, 'description'):
            processor.setDescription(self.ProcessorDetails.description)
        else:
            processor.setDescription(self.__class__.__name__)

        if hasattr(self, 'ProcessorDetails') and hasattr(self.ProcessorDetails, 'version'):
            processor.setVersion(self.ProcessorDetails.version)

    def onInitialize(self, processor: Processor):
        get_dynamic_property_descriptor_attr = getattr(self, 'getDynamicPropertyDescriptor', None)
        if get_dynamic_property_descriptor_attr and callable(get_dynamic_property_descriptor_attr):
            processor.setSupportsDynamicProperties()
            self.supports_dynamic_properties = True
        else:
            self.supports_dynamic_properties = False

        for property in self.getPropertyDescriptors():
            expression_language_supported = True if property.expressionLanguageScope != ExpressionLanguageScope.NONE else False
            property_type_code = translateStandardValidatorToMiNiFiPropertype(property.validators)

            # MiNiFi C++ does not support validators for expression language enabled properties
            if expression_language_supported and property_type_code is not None and property_type_code != MinifiPropertyTypes.NON_BLANK_TYPE:
                self.logger.warn("Property '{}' has validators defined, but since it also supports Expression Language, the validators will be ignored.".format(property.name))
                property_type_code = None

            # MiNiFi C++ does not support dependant properties, so if a property depends on another property, it should not be required
            is_required = True if property.required and not property.dependencies else False
            processor.addProperty(property.name, property.description, property.defaultValue, is_required, expression_language_supported,
                                  property.sensitive, property_type_code, property.allowableValues, property.controllerServiceDefinition)

    def onScheduled(self, context_proxy: ProcessContextProxy):
        pass

    def onSchedule(self, context: ProcessContext):
        context_proxy = ProcessContextProxy(context, self)
        self.onScheduled(context_proxy)

    @abstractmethod
    def onTrigger(self, context: ProcessContext, session: ProcessSession):
        pass

    def getPropertyDescriptors(self) -> List[PropertyDescriptor]:
        return []

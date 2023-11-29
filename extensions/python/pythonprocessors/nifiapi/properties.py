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

from enum import Enum
from typing import List
from minifi_native import ProcessSession, FlowFile, ProcessContext, timePeriodStringToMilliseconds, dataSizeStringToBytes


# This is a mock for NiFi's StandardValidators class methods, that return the property type equivalent in MiNiFi C++ if exists
class ValidatorGenerator:
    def createNonNegativeFloatingPointValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createDirectoryExistsValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createURLValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createListValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createTimePeriodValidator(self, *args) -> int:
        return StandardValidators.TIME_PERIOD_VALIDATOR

    def createAttributeExpressionLanguageValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createDataSizeBoundsValidator(self, *args) -> int:
        return StandardValidators.DATA_SIZE_VALIDATOR

    def createRegexMatchingValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createRegexValidator(self, *args) -> int:
        return StandardValidators.ALWAYS_VALID

    def createLongValidator(self, *args) -> int:
        return StandardValidators.LONG_VALIDATOR


class StandardValidators:
    _standard_validators = ValidatorGenerator()

    ALWAYS_VALID = 0
    NON_EMPTY_VALIDATOR = 1
    INTEGER_VALIDATOR = 2
    POSITIVE_INTEGER_VALIDATOR = 3
    POSITIVE_LONG_VALIDATOR = 4
    NON_NEGATIVE_INTEGER_VALIDATOR = 5
    NUMBER_VALIDATOR = 6
    LONG_VALIDATOR = 7
    PORT_VALIDATOR = 8
    NON_EMPTY_EL_VALIDATOR = 9
    HOSTNAME_PORT_LIST_VALIDATOR = 10
    BOOLEAN_VALIDATOR = 11
    URL_VALIDATOR = 12
    URI_VALIDATOR = 13
    REGULAR_EXPRESSION_VALIDATOR = 14
    REGULAR_EXPRESSION_WITH_EL_VALIDATOR = 15
    TIME_PERIOD_VALIDATOR = 16
    DATA_SIZE_VALIDATOR = 17
    FILE_EXISTS_VALIDATOR = 18
    NON_NEGATIVE_FLOATING_POINT_VALIDATOR = 19


class MinifiPropertyTypes:
    INTEGER_TYPE = 0
    LONG_TYPE = 1
    BOOLEAN_TYPE = 2
    DATA_SIZE_TYPE = 3
    TIME_PERIOD_TYPE = 4
    NON_BLANK_TYPE = 5
    PORT_TYPE = 6


def translateStandardValidatorToMiNiFiPropertype(validators: List[int]) -> int:
    if validators is None or len(validators) == 0 or len(validators) > 1:
        return None

    validator = validators[0]
    if validator == StandardValidators.INTEGER_VALIDATOR:
        return MinifiPropertyTypes.INTEGER_TYPE
    if validator == StandardValidators.LONG_VALIDATOR:
        return MinifiPropertyTypes.LONG_TYPE
    if validator == StandardValidators.BOOLEAN_VALIDATOR:
        return MinifiPropertyTypes.BOOLEAN_TYPE
    if validator == StandardValidators.DATA_SIZE_VALIDATOR:
        return MinifiPropertyTypes.DATA_SIZE_TYPE
    if validator == StandardValidators.TIME_PERIOD_VALIDATOR:
        return MinifiPropertyTypes.TIME_PERIOD_TYPE
    if validator == StandardValidators.NON_EMPTY_VALIDATOR:
        return MinifiPropertyTypes.NON_BLANK_TYPE
    if validator == StandardValidators.PORT_VALIDATOR:
        return MinifiPropertyTypes.PORT_TYPE
    return None


class PropertyDependency:
    def __init__(self, property_descriptor, *dependent_values):
        if dependent_values is None:
            dependent_values = []

        self.property_descriptor = property_descriptor
        self.dependent_values = dependent_values


class ResourceDefinition:
    def __init__(self, allow_multiple=False, allow_file=True, allow_url=False, allow_directory=False, allow_text=False):
        self.allow_multiple = allow_multiple
        self.allow_file = allow_file
        self.allow_url = allow_url
        self.allow_directory = allow_directory
        self.allow_text = allow_text


class ExpressionLanguageScope(Enum):
    NONE = 1
    ENVIRONMENT = 2
    FLOWFILE_ATTRIBUTES = 3


class PropertyDescriptor:
    def __init__(self, name: str, description: str, required: bool = False, sensitive: bool = False,
                 display_name: str = None, default_value: str = None, allowable_values: List[str] = None,
                 dependencies: List[PropertyDependency] = None, expression_language_scope: ExpressionLanguageScope = ExpressionLanguageScope.NONE,
                 dynamic: bool = False, validators: List[int] = None, resource_definition: ResourceDefinition = None, controller_service_definition: str = None):
        if validators is None:
            validators = [StandardValidators.ALWAYS_VALID]

        self.name = name
        self.description = description
        self.required = required
        self.sensitive = sensitive
        self.displayName = display_name
        self.defaultValue = default_value
        self.allowableValues = allowable_values
        self.dependencies = dependencies
        self.expressionLanguageScope = expression_language_scope
        self.dynamic = dynamic
        self.validators = validators
        self.resourceDefinition = resource_definition
        self.controllerServiceDefinition = controller_service_definition


class TimeUnit(Enum):
    NANOSECONDS = "NANOSECONDS",
    MICROSECONDS = "MICROSECONDS",
    MILLISECONDS = "MILLISECONDS",
    SECONDS = "SECONDS",
    MINUTES = "MINUTES",
    HOURS = "HOURS",
    DAYS = "DAYS"


class DataUnit(Enum):
    B = "B",
    KB = "KB",
    MB = "MB",
    GB = "GB",
    TB = "TB"


class FlowFileProxy:
    def __init__(self, session: ProcessSession, flow_file: FlowFile):
        self.session = session
        self.flow_file = flow_file

    def getContentsAsBytes(self):
        return self.session.getContentsAsBytes(self.flow_file)

    def getAttribute(self, name: str):
        return self.flow_file.getAttribute(name)

    def getSize(self):
        return self.flow_file.getSize()

    def getAttributes(self):
        return self.flow_file.getAttributes()


class PythonPropertyValue:
    def __init__(self, cpp_context: ProcessContext, name: str, string_value: str, el_supported: bool):
        self.cpp_context = cpp_context
        self.value = None
        self.name = name
        if string_value is not None:
            self.value = string_value
        self.el_supported = el_supported

    def getValue(self) -> str:
        return self.value

    def isSet(self) -> bool:
        return self.value is not None

    def asInteger(self) -> int:
        if not self.value:
            return None
        return int(self.value)

    def asBoolean(self) -> bool:
        if not self.value:
            return None
        return self.value.lower() == 'true'

    def asFloat(self) -> float:
        if not self.value:
            return None
        return float(self.value)

    def asTimePeriod(self, time_unit: TimeUnit) -> int:
        if not self.value:
            return None
        milliseconds = timePeriodStringToMilliseconds(self.value)
        if time_unit == TimeUnit.NANOSECONDS:
            return milliseconds * 1000000
        if time_unit == TimeUnit.MICROSECONDS:
            return milliseconds * 1000
        if time_unit == TimeUnit.MILLISECONDS:
            return milliseconds
        if time_unit == TimeUnit.SECONDS:
            return int(round(milliseconds / 1000))
        if time_unit == TimeUnit.MINUTES:
            return int(round(milliseconds / 1000 / 60))
        if time_unit == TimeUnit.HOURS:
            return int(round(milliseconds / 1000 / 60 / 60))
        if time_unit == TimeUnit.DAYS:
            return int(round(milliseconds / 1000 / 60 / 60 / 24))
        return 0

    def asDataSize(self, data_unit: DataUnit) -> int:
        if not self.value:
            return None
        bytes = dataSizeStringToBytes(self.value)
        if data_unit == DataUnit.B:
            return bytes
        if data_unit == DataUnit.KB:
            return int(bytes / 1000)
        if data_unit == DataUnit.MB:
            return int(bytes / 1000 / 1000)
        if data_unit == DataUnit.GB:
            return int(bytes / 1000 / 1000 / 1000)
        if data_unit == DataUnit.TB:
            return int(bytes / 1000 / 1000 / 1000 / 1000)
        return 0

    def evaluateAttributeExpressions(self, flow_file_proxy: FlowFileProxy):
        # If Expression Language is supported and present, evaluate it and return a new PropertyValue.
        # Otherwise just return self, in order to avoid the cost of making the call to cpp for getProperty
        if self.el_supported:
            new_string_value = self.cpp_context.getProperty(self.name, flow_file_proxy.flow_file)
            return PythonPropertyValue(self.cpp_context, self.name, new_string_value, self.el_supported)

        return self


class ProcessContextProxy:
    def __init__(self, cpp_context: ProcessContext):
        self.cpp_context = cpp_context

    def getProperty(self, descriptor: PropertyDescriptor) -> PythonPropertyValue:
        property_value = self.cpp_context.getProperty(descriptor.name)
        return PythonPropertyValue(self.cpp_context, descriptor.name, property_value, descriptor.expressionLanguageScope != ExpressionLanguageScope.NONE)

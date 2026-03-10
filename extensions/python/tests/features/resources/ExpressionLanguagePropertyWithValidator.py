#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult
from nifiapi.properties import ExpressionLanguageScope, PropertyDescriptor, StandardValidators


class ExpressionLanguagePropertyWithValidator(FlowFileTransform):

    class Java:
        implements = ['org.apache.nifi.python.processor.FlowFileTransform']

    class ProcessorDetails:
        version = '1.2.3'
        description = "Test processor"
        dependencies = []

    INTEGER_PROPERTY = PropertyDescriptor(
        name="Integer Property",
        description="A dummy integer property",
        required=True,
        validators=[StandardValidators.INTEGER_VALIDATOR],
        expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES
    )

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return [self.INTEGER_PROPERTY]

    def getDynamicPropertyDescriptor(self, propertyname):
        return PropertyDescriptor(name=propertyname,
                                  description="A user-defined property",
                                  dynamic=True)

    def transform(self, context, flow_file):
        integer_value = context.getProperty(self.INTEGER_PROPERTY).asInteger()
        self.logger.info("Integer Property value: {}".format(integer_value))

        return FlowFileTransformResult('success', contents="content")

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

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult
from nifiapi.properties import PropertyDescriptor, ExpressionLanguageScope


class EvaluateExpressionLanguageChecker(FlowFileTransform):
    class Java:
        implements = ["org.apache.nifi.python.processor.FlowFileTransform"]

    class ProcessorDetails:
        version = "0.1.0"
        description = "Processor to check property evaluates with expression language supported and not supported"

    EL_PROPERTY = PropertyDescriptor(
        name="EL Property",
        description="EL property",
        expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES
    )

    NON_EL_PROPERTY = PropertyDescriptor(
        name="Non EL Property",
        description="Non EL property",
    )

    property_descriptors = [
        EL_PROPERTY,
        NON_EL_PROPERTY
    ]

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return self.property_descriptors

    def getDynamicPropertyDescriptor(self, propertyname):
        return PropertyDescriptor(name=propertyname,
                                  description="A user-defined property",
                                  dynamic=True)

    def transform(self, context, flowFile):
        el_property = context.getProperty(self.EL_PROPERTY)
        el_property_value = el_property.getValue()
        self.logger.info("EL Property value: " + str(el_property_value))

        el_property_value_evaluated = el_property.evaluateAttributeExpressions(flowFile).getValue()
        self.logger.info("Evaluated EL Property value: " + str(el_property_value_evaluated))

        non_el_property = context.getProperty(self.NON_EL_PROPERTY)
        non_el_property_value = non_el_property.getValue()
        self.logger.info("Non EL Property value: " + str(non_el_property_value))

        non_el_property_value_evaluated = non_el_property.evaluateAttributeExpressions(flowFile).getValue()
        self.logger.info("Evaluated Non EL Property value: " + str(non_el_property_value_evaluated))

        non_existent_value = context.getProperty("non-existent-property").evaluateAttributeExpressions(flowFile).getValue()
        if non_existent_value is None:
            self.logger.info("Non-existent property value is empty")

        dynamic_property = context.getProperty("My Dynamic Property")
        dynamic_property_value = dynamic_property.getValue()
        if dynamic_property_value:
            self.logger.info("My Dynamic Property value is: " + str(dynamic_property_value))

        dynamic_property_evaluated_value = dynamic_property.evaluateAttributeExpressions(flowFile).getValue()
        if dynamic_property_evaluated_value:
            self.logger.info("My Dynamic Property evaluated value is: " + str(dynamic_property_evaluated_value))

        return FlowFileTransformResult("success", contents="Check successful!")

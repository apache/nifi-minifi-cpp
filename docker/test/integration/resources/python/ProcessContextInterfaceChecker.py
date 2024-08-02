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
from nifiapi.properties import PropertyDescriptor, StandardValidators
from nifiapi.relationship import Relationship


class ProcessContextInterfaceChecker(FlowFileTransform):
    """
    Checks ProcessContext interface
    """
    SECRET_PASSWORD = PropertyDescriptor(
        name="Secret Password",
        description="Password to access",
        validators=[StandardValidators.NON_EMPTY_VALIDATOR],
        default_value="mysecret",
        required=True,
        sensitive=True
    )
    REQUEST_TIMEOUT = PropertyDescriptor(
        name="Request Timeout",
        description="Timeout for the request",
        validators=[StandardValidators.TIME_PERIOD_VALIDATOR],
        default_value="60 sec",
        required=True
    )
    WISH_COUNT = PropertyDescriptor(
        name="Wish Count",
        description="Number of wishes",
        default_value="3",
        validators=[StandardValidators.POSITIVE_INTEGER_VALIDATOR],
        required=True
    )

    property_descriptors = [
        SECRET_PASSWORD,
        REQUEST_TIMEOUT,
        WISH_COUNT
    ]

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return self.property_descriptors

    def getRelationships(self):
        return [Relationship("myrelationship", "Dummy dynamic relationship")]

    def transform(self, context, flowFile):
        properties = context.getProperties()
        if len(properties) != 3:
            return FlowFileTransformResult("failure")

        property_names = [property.name for property in properties]
        if "Secret Password" not in property_names or "Request Timeout" not in property_names or "Wish Count" not in property_names:
            return FlowFileTransformResult("failure")

        for property in properties:
            if property.name == "Secret Password" and properties[property] != "mysecret":
                return FlowFileTransformResult("failure")
            elif property.name == "Request Timeout" and properties[property] != "60 sec":
                return FlowFileTransformResult("failure")
            elif property.name == "Wish Count" and properties[property] != "3":
                return FlowFileTransformResult("failure")

        secret_password = context.getProperty(self.SECRET_PASSWORD).getValue()
        if secret_password != "mysecret":
            return FlowFileTransformResult("failure")

        timeout = context.getProperty(self.REQUEST_TIMEOUT).getValue()
        if timeout != "60 sec":
            return FlowFileTransformResult("failure")

        wish_count = context.getProperty(self.WISH_COUNT).getValue()
        if wish_count != "3":
            return FlowFileTransformResult("failure")

        return FlowFileTransformResult("myrelationship", contents="Check successful!")

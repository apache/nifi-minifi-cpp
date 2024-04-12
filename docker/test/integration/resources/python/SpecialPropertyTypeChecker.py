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
from nifiapi.properties import PropertyDescriptor, TimeUnit, DataUnit


class SpecialPropertyTypeChecker(FlowFileTransform):
    """
    Checks if custom property types are working as expected
    """
    TIME_PERIOD_PROPERTY = PropertyDescriptor(
        name="Time Period Property",
        description="Dummy property that should be a time period",
        default_value="30 sec",
        required=True
    )
    DATA_SIZE_PROPERTY = PropertyDescriptor(
        name="Data Size Property",
        description="Dummy property that should be a data size",
        default_value="100 MB",
        required=True
    )

    property_descriptors = [
        TIME_PERIOD_PROPERTY,
        DATA_SIZE_PROPERTY
    ]

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return self.property_descriptors

    def transform(self, context, flowFile):
        time_in_ms = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.MILLISECONDS)
        if time_in_ms != 30000:
            self.logger.error("Time period property is not working as expected")
            return FlowFileTransformResult("failure", contents="Time period property is not working as expected")

        data_size_in_kbytes = context.getProperty(self.DATA_SIZE_PROPERTY).asDataSize(DataUnit.KB)
        if data_size_in_kbytes != 102400.0:
            self.logger.error("Data size property is not working as expected")
            return FlowFileTransformResult("failure", contents="Data size property is not working as expected")

        return FlowFileTransformResult("success", contents="Check successful!")

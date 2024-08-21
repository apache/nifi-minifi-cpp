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
        default_value="2 hours",
        required=True
    )
    DATA_SIZE_PROPERTY = PropertyDescriptor(
        name="Data Size Property",
        description="Dummy property that should be a data size",
        default_value="100 MB",
        required=True
    )
    SSL_CONTEXT_PROPERTY = PropertyDescriptor(
        name="SSL Context Service",
        description="Dummy property that should be an SSL Context",
        controller_service_definition="SSLContextService",
        required=True
    )

    property_descriptors = [
        TIME_PERIOD_PROPERTY,
        DATA_SIZE_PROPERTY,
        SSL_CONTEXT_PROPERTY
    ]

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return self.property_descriptors

    def transform(self, context, flowFile):
        time_in_microseconds = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.MICROSECONDS)
        if time_in_microseconds != 7200000000:
            self.logger.error("Time period property conversion to microseconds is not working as expected")
            return FlowFileTransformResult("failure")

        time_in_milliseconds = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.MILLISECONDS)
        if time_in_milliseconds != 7200000:
            self.logger.error("Time period property conversion to milliseconds is not working as expected")
            return FlowFileTransformResult("failure")

        time_in_seconds = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.SECONDS)
        if time_in_seconds != 7200:
            self.logger.error("Time period property conversion to seconds is not working as expected")
            return FlowFileTransformResult("failure")

        time_in_minutes = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.MINUTES)
        if time_in_minutes != 120:
            self.logger.error("Time period property conversion to minutes is not working as expected")
            return FlowFileTransformResult("failure")

        time_in_hours = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.HOURS)
        if time_in_hours != 2:
            self.logger.error("Time period property conversion to hours is not working as expected")
            return FlowFileTransformResult("failure")

        time_in_days = context.getProperty(self.TIME_PERIOD_PROPERTY).asTimePeriod(TimeUnit.DAYS)
        if time_in_days != 0:
            self.logger.error("Time period property conversion to days is not working as expected")
            return FlowFileTransformResult("failure")

        data_size_in_bytes = context.getProperty(self.DATA_SIZE_PROPERTY).asDataSize(DataUnit.B)
        if data_size_in_bytes != 104857600.0:
            self.logger.error("Data size property conversion to bytes is not working as expected")
            return FlowFileTransformResult("failure")

        data_size_in_kilobytes = context.getProperty(self.DATA_SIZE_PROPERTY).asDataSize(DataUnit.KB)
        if data_size_in_kilobytes != 102400.0:
            self.logger.error("Data size property conversion to kilobytes is not working as expected")
            return FlowFileTransformResult("failure")

        data_size_in_megabytes = context.getProperty(self.DATA_SIZE_PROPERTY).asDataSize(DataUnit.MB)
        if data_size_in_megabytes != 100.0:
            self.logger.error("Data size property conversion to megabytes is not working as expected")
            return FlowFileTransformResult("failure")

        data_size_in_gigabytes = context.getProperty(self.DATA_SIZE_PROPERTY).asDataSize(DataUnit.GB)
        if data_size_in_gigabytes != 0.09765625:
            self.logger.error("Data size property conversion to gigabytes is not working as expected")
            return FlowFileTransformResult("failure")

        ssl_context = context.getProperty(self.SSL_CONTEXT_PROPERTY).asControllerService()
        cert = ssl_context.getCertificateFile()
        if cert != "/tmp/resources/minifi_client.crt":
            self.logger.error("SSL Context Service property is not working as expected")
            return FlowFileTransformResult("failure")

        return FlowFileTransformResult("success", contents="Check successful!")

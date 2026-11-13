# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from typing import ClassVar

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult
from nifiapi.properties import (
    ExpressionLanguageScope,
    PropertyDescriptor,
    StandardValidators,
)


class NifiStyleLogDynamicProperties(FlowFileTransform):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileTransform"]

    class ProcessorDetails:
        version = "1.2.3"
        description = "Test processor"
        dependencies: ClassVar[list] = []

    STATIC_PROPERTY = PropertyDescriptor(
        name="Static Property",
        description="A dummy static property",
        required=True,
        validators=[StandardValidators.NON_EMPTY_VALIDATOR],
        expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES,
    )

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return [self.STATIC_PROPERTY]

    def getDynamicPropertyDescriptor(self, propertyname):
        return PropertyDescriptor(name=propertyname, description="A user-defined property", dynamic=True)

    def transform(self, context, flow_file):
        property_value = context.getProperty("Static Property")
        self.logger.info(f"Static Property value: {property_value.getValue()}")
        dyn_property_value = context.getProperty("Dynamic Property")
        self.logger.info(f"Dynamic Property value: {dyn_property_value.getValue()}")

        return FlowFileTransformResult("success", contents="content")

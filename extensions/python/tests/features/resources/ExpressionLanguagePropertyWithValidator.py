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


class ExpressionLanguagePropertyWithValidator(FlowFileTransform):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.FlowFileTransform"]

    class ProcessorDetails:
        version = "1.2.3"
        description = "Test processor"
        dependencies: ClassVar[list] = []

    INTEGER_PROPERTY = PropertyDescriptor(
        name="Integer Property",
        description="A dummy integer property",
        required=True,
        validators=[StandardValidators.INTEGER_VALIDATOR],
        expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES,
    )

    def __init__(self, **kwargs):
        pass

    def getPropertyDescriptors(self):
        return [self.INTEGER_PROPERTY]

    def getDynamicPropertyDescriptor(self, propertyname):
        return PropertyDescriptor(name=propertyname, description="A user-defined property", dynamic=True)

    def transform(self, context, flow_file):
        integer_value = context.getProperty(self.INTEGER_PROPERTY).asInteger()
        self.logger.info(f"Integer Property value: {integer_value}")

        return FlowFileTransformResult("success", contents="content")

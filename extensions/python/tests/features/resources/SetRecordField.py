# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from typing import ClassVar

from nifiapi.properties import (
    ExpressionLanguageScope,
    PropertyDescriptor,
    StandardValidators,
)
from nifiapi.recordtransform import RecordTransform, RecordTransformResult


class SetRecordField(RecordTransform):
    class Java:
        implements: ClassVar[list] = ["org.apache.nifi.python.processor.RecordTransform"]

    class ProcessorDetails:
        version = "0.0.1-SNAPSHOT"

    def __init__(self, **kwargs):
        super().__init__()

    def transform(self, context, record, schema, attributemap):
        # Update dictionary based on the dynamic properties provided by user
        for key in context.getProperties():
            if not key.dynamic:
                continue

            propname = key.name
            record[propname] = context.getProperty(propname).evaluateAttributeExpressions(attributemap).getValue()

        # Determine the partition
        if "group" in record:
            partition = {"group": record["group"]}
        else:
            partition = None

        # Return the result
        return RecordTransformResult(record=record, relationship="success", partition=partition)

    def getDynamicPropertyDescriptor(self, name):
        return PropertyDescriptor(
            name=name,
            description="Specifies the value to set for the '" + name + "' field",
            expression_language_scope=ExpressionLanguageScope.FLOWFILE_ATTRIBUTES,
            validators=[StandardValidators.ALWAYS_VALID],
        )

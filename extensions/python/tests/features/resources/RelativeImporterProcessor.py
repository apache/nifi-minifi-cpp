# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult

from ..subtractutils import minus_ten
from .multiplierutils import double


class RelativeImporterProcessor(FlowFileTransform):
    def __init__(self, **kwargs):
        pass

    def transform(self, context, flowFile):
        number = 1000
        number = double(number)
        number = minus_ten(number)
        return FlowFileTransformResult("success", contents=f"The final result is {number}")

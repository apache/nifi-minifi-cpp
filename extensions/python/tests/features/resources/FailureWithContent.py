# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from nifiapi.flowfiletransform import FlowFileTransform, FlowFileTransformResult


class FailureWithContent(FlowFileTransform):
    def __init__(self, **kwargs):
        pass

    def transform(self, context, flowFile):
        return FlowFileTransformResult("failure", contents="Content?")

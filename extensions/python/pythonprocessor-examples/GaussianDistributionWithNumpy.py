# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import numpy as np


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        output_stream.write(self.content.encode("utf-8"))
        return len(self.content)


def describe(processor):
    processor.setDescription("Draw random samples from a normal (Gaussian) distribution.")


def onInitialize(processor):
    pass


def onTrigger(context, session):
    flow_file = session.create()
    mu = 0
    sigma = 0.1
    s = np.random.normal(mu, sigma, 1)
    session.write(flow_file, WriteCallback(str(s[0])))
    session.transfer(flow_file, REL_SUCCESS)

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

import numpy as np


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        output_stream.write(self.content.encode('utf-8'))
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

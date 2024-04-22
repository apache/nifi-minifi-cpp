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

global i
i = 0


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        output_stream.write(self.content.encode('utf-8'))
        return len(self.content)


def describe(processor):
    processor.setDescription("Counts up with the help of a global variable")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


def onTrigger(context, session):
    global i
    i = i + 1
    flow_file = session.create()
    session.write(flow_file, WriteCallback(str(i)))
    session.transfer(flow_file, REL_SUCCESS)

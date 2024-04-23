#!/usr/bin/env python
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
import codecs
import json


class ReadCallback:
    def process(self, input_stream):
        self.content = codecs.getreader('utf-8')(input_stream).read()
        return len(self.content)


class WriteToJsonCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        json_content = json.dumps({"content": self.content})
        output_stream.write(json_content.encode('utf-8'))
        return len(json_content)


def describe(processor):
    processor.setDescription("Moves content of flow file to JSON file under 'content' key")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


def onTrigger(context, session):
    flow_file = session.get()
    if flow_file is not None:
        read_callback = ReadCallback()
        session.read(flow_file, read_callback)
        session.write(flow_file, WriteToJsonCallback(read_callback.content))
        session.transfer(flow_file, REL_SUCCESS)

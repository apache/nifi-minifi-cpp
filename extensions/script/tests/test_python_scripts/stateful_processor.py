#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.


def describe(processor):
    processor.setDescription("Processor used for testing in ExecutePythonProcessorTests.cpp")


state = 0


class WriteCallback(object):
    def process(self, output_stream):
        global state
        new_content = str(state).encode('utf-8')
        output_stream.write(new_content)
        state = state + 1
        return len(new_content)


def onTrigger(context, session):
    global state
    log.info('Vrrm, vrrrm, processor is running, vrrrm!!')
    # flow_file = session.get()
    flow_file = session.create()
    flow_file.setAttribute("filename", str(state))
    log.info('created flow file: %s' % flow_file.getAttribute('filename'))

    if flow_file is not None:
        session.write(flow_file, WriteCallback())
        session.transfer(flow_file, REL_SUCCESS)

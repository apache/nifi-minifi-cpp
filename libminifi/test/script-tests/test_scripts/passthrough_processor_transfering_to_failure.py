#
#
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
#


def describe(processor):
  processor.setDescription("Processor used for testing in ExecutePythonProcessorTests.cpp")


def onTrigger(context, session):
  flow_file = session.get()
  log.info('Vrrm, vrrrm, processor is running, vrrrm!!')  # noqa: F821

  if flow_file is not None:
    log.info('created flow file: %s' % flow_file.getAttribute('filename'))  # noqa: F821
    session.transfer(flow_file, REL_FAILURE)  # noqa: F821

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


import uuid
import logging


class ControllerService(object):
    def __init__(self, name=None, properties=None):

        self.id = str(uuid.uuid4())
        self.instance_id = str(uuid.uuid4())

        if name is None:
            self.name = str(uuid.uuid4())
            logging.info('Controller service name was not provided; using generated name \'%s\'', self.name)
        else:
            self.name = name

        if properties is None:
            properties = {}

        self.properties = properties
        self.linked_services = []

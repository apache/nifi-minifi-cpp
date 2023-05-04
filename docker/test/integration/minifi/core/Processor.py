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


from .Connectable import Connectable


class Processor(Connectable):
    def __init__(self,
                 context,
                 clazz,
                 properties=None,
                 schedule=None,
                 name=None,
                 controller_services=None,
                 auto_terminate=None,
                 class_prefix='org.apache.nifi.processors.standard.',
                 max_concurrent_tasks=1):

        super(Processor, self).__init__(name=name,
                                        auto_terminate=auto_terminate)

        self.context = context
        self.class_prefix = class_prefix

        if controller_services is None:
            controller_services = []

        if schedule is None:
            schedule = {}

        if properties is None:
            properties = {}

        self.clazz = clazz

        self.properties = properties
        self.controller_services = controller_services
        self.max_concurrent_tasks = max_concurrent_tasks

        self.schedule = {
            'scheduling strategy': 'TIMER_DRIVEN',
            'scheduling period': '1 sec',
            'penalization period': '30 sec',
            'yield period': '1 sec',
            'run duration nanos': 0
        }
        self.schedule.update(schedule)

    def set_property(self, key, value):
        if value.isdigit():
            self.properties[key] = int(value)
        else:
            self.properties[key] = value

    def set_max_concurrent_tasks(self, max_concurrent_tasks):
        self.max_concurrent_tasks = max_concurrent_tasks

    def unset_property(self, key):
        self.properties.pop(key, None)

    def set_scheduling_strategy(self, value):
        self.schedule["scheduling strategy"] = value

    def set_scheduling_period(self, value):
        self.schedule["scheduling period"] = value

    def nifi_property_key(self, key):
        """
        Returns the Apache NiFi-equivalent property key for the given key. This is often, but not always, the same as
        the internal key.
        """
        return key

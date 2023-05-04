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


from ..core.Processor import Processor


class QuerySplunkIndexingStatus(Processor):
    def __init__(self, context, schedule={'scheduling strategy': 'EVENT_DRIVEN', 'penalization period': '1 sec'}):
        super(QuerySplunkIndexingStatus, self).__init__(
            context=context,
            clazz='QuerySplunkIndexingStatus',
            properties={
                'Hostname': 'splunk',
                'Port': '8088',
                'Token': 'Splunk 176fae97-f59d-4f08-939a-aa6a543f2485'
            },
            auto_terminate=['acknowledged', 'unacknowledged', 'undetermined', 'failure'],
            schedule=schedule)

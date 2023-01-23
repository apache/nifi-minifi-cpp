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
from utils import wait_for


class PostgresChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    def __query_postgres_server(self, postgresql_container_name, query, number_of_rows):
        (code, output) = self.container_communicator.execute_command(postgresql_container_name, ["psql", "-U", "postgres", "-c", query])
        return code == 0 and str(number_of_rows) + " rows" in output

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        return wait_for(lambda: self.__query_postgres_server(postgresql_container_name, query, number_of_rows), timeout_seconds)

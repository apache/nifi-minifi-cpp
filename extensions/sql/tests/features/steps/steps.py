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

import humanfriendly
from behave import step, given, then

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.minifi.controller_service import ControllerService
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from containers.postgress_server_container import PostgresContainer


@given("an ODBCService is setup up for {processor_name} with the name \"{service_name}\"")
def step_impl(context: MinifiTestContext, processor_name: str, service_name: str):
    odb_service = ControllerService(class_name="ODBCService", service_name=service_name)
    postgres_server_hostname = f"postgres-server-{context.scenario_id}"
    odb_service.add_property("Connection String", f"Driver={{PostgreSQL ANSI}};Server={postgres_server_hostname};Port=5432;Database=postgres;Uid=postgres;Pwd=password;")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(odb_service)
    processor = context.get_or_create_default_minifi_container().flow_definition.get_processor(processor_name)
    processor.add_property("DB Controller Service", "ODBCService")


@step("a PostgreSQL server is set up")
def step_impl(context):
    context.containers["postgres-server"] = PostgresContainer(context)


@then('the query "{query}" returns {rows:d} rows in less than {timeout_str} on the PostgreSQL server')
def step_impl(context, query: str, rows: int, timeout_str: str):
    timeout_seconds = humanfriendly.parse_timespan(timeout_str)
    postgres_container = context.containers["postgres-server"]
    assert isinstance(postgres_container, PostgresContainer)
    assert wait_for_condition(
        condition=lambda: postgres_container.check_query_results(query, int(rows)),
        timeout_seconds=timeout_seconds,
        bail_condition=lambda: postgres_container.exited,
        context=context)

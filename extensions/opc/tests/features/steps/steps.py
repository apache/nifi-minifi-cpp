import humanfriendly
from behave import step, then

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.helpers import wait_for_condition
from opc_ua_server_container import OPCUAServerContainer


@step("an OPC UA server is set up")
def step_impl(context: MinifiTestContext):
    context.containers["opcua-server"] = OPCUAServerContainer(context)


@step("an OPC UA server is set up with access control")
def step_impl(context: MinifiTestContext):
    context.containers["opcua-server-access"] = OPCUAServerContainer(context, command=["/opt/open62541/examples/access_control_server"])


@then("the OPC UA server logs contain the following message: \"{log_message}\" in less than {duration}")
def step_impl(context, log_message, duration):
    timeout_seconds = humanfriendly.parse_timespan(duration)
    opcua_container = context.containers["opcua-server"]
    assert isinstance(opcua_container, OPCUAServerContainer)
    assert wait_for_condition(
        condition=lambda: log_message in opcua_container.get_logs(),
        timeout_seconds=timeout_seconds,
        bail_condition=lambda: opcua_container.exited,
        context=context)

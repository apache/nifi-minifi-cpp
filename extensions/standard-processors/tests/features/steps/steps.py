from behave import step

from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from syslog_container import SyslogContainer


@step("a Syslog client with TCP protocol is setup to send logs to minifi")
def step_impl(context: MinifiTestContext):
    context.containers["syslog-tcp"] = SyslogContainer("tcp", context)


@step("a Syslog client with UDP protocol is setup to send logs to minifi")
def step_impl(context):
    context.containers["syslog-udp"] = SyslogContainer("udp", context)

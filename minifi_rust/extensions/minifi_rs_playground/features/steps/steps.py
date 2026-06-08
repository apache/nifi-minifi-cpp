from behave import then, when, given
import humanfriendly

from minifi_behave.steps import checking_steps  # noqa: F401
from minifi_behave.steps import configuration_steps  # noqa: F401
from minifi_behave.steps import core_steps  # noqa: F401
from minifi_behave.steps import flow_building_steps  # noqa: F401
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext


@when("the MiNiFi instance is started without assertions")
def minifi_starts_wo_assertions(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().deploy(context)


@then('Minifi crashes with the following "{crash_msg}" in less than {duration}')
def minifi_crashes(context: MinifiTestContext, crash_msg: str, duration: str):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: (
            context.get_or_create_default_minifi_container().exited
            and crash_msg in context.get_or_create_default_minifi_container().get_logs()
        ),
        timeout_seconds=duration_seconds,
        bail_condition=lambda: False,
        context=context,
    )


@given("MiNiFi logs processor metrics")
def minifi_logs_processor_metrics(context: MinifiTestContext):
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.LogMetricsPublisher.metrics",
        "GetFileRsMetrics,DuplicateStreamTextMetrics,PutFileRsMetrics",
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.LogMetricsPublisher.logging.interval", "1s"
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.class", "LogMetricsPublisher"
    )
    context.get_or_create_default_minifi_container().set_property(
        "nifi.metrics.publisher.agent.identifier", "Agent1"
    )

import humanfriendly
from behave import step, then

from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.steps import checking_steps        # noqa: F401
from minifi_test_framework.steps import configuration_steps   # noqa: F401
from minifi_test_framework.steps import core_steps            # noqa: F401
from minifi_test_framework.steps import flow_building_steps   # noqa: F401
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.minifi.processor import Processor
from azure_server_container import AzureServerContainer


@step("a {processor_name} processor set up to communicate with an Azure blob storage")
def step_impl(context: MinifiTestContext, processor_name: str):
    processor = Processor(processor_name, processor_name)
    hostname = f"http://azure-storage-server-{context.scenario_id}"
    processor.add_property('Container Name', 'test-container')
    processor.add_property('Connection String', 'DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint={hostname}:10000/devstoreaccount1;QueueEndpoint={hostname}:10001/devstoreaccount1;'.format(hostname=hostname))
    processor.add_property('Blob', 'test-blob')
    processor.add_property('Create Container', 'true')
    context.minifi_container.flow_definition.add_processor(processor)


@step("an Azure storage server is set up")
def step_impl(context):
    context.containers.append(AzureServerContainer(context))


@then('the object on the Azure storage server is "{object_data}"')
def step_impl(context: MinifiTestContext, object_data: str):
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert azure_server_container.check_azure_storage_server_data(object_data)


@step('test blob "{blob_name}" with the content "{data}" is created on Azure blob storage')
def step_impl(context: MinifiTestContext, blob_name: str, data: str):
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert azure_server_container.add_test_blob(blob_name, content=data)


@step('test blob "{blob_name}" is created on Azure blob storage')
def step_impl(context: MinifiTestContext, blob_name: str):
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert azure_server_container.add_test_blob(blob_name)


@step('test blob "{blob_name}" is created on Azure blob storage with a snapshot')
def step_impl(context: MinifiTestContext, blob_name: str):
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert azure_server_container.add_test_blob(blob_name, with_snapshot=True)


@then("the Azure blob storage becomes empty in {timeout_str}")
def step_impl(context: MinifiTestContext, timeout_str: str):
    timeout_in_seconds = humanfriendly.parse_timespan(timeout_str)
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert wait_for_condition(
        condition=lambda: azure_server_container.check_azure_blob_storage_is_empty(),
        timeout_seconds=timeout_in_seconds,
        bail_condition=lambda: context.minifi_container.exited,
        context=context)


@then("the blob and snapshot count becomes 1 in {timeout_str}")
def step_impl(context: MinifiTestContext, timeout_str: str):
    timeout_in_seconds = humanfriendly.parse_timespan(timeout_str)
    azure_server_container = context.containers[0]
    assert isinstance(azure_server_container, AzureServerContainer)
    assert wait_for_condition(
        condition=lambda: azure_server_container.check_azure_blob_and_snapshot_count(1),
        timeout_seconds=timeout_in_seconds,
        bail_condition=lambda: context.minifi_container.exited,
        context=context)

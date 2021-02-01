from behave import fixture, use_fixture
import sys
sys.path.append('../minifi')
import logging

from MiNiFi_integration_test_driver import MiNiFi_integration_test
from minifi import *

def raise_exception(exception):
    raise exception

@fixture
def test_driver_fixture(context):
    logging.info("Integration test setup")
    context.test = MiNiFi_integration_test(context)
    yield context.test
    logging.info("Integration test teardown...")
    del context.test

def before_scenario(context, scenario):
    use_fixture(test_driver_fixture, context)

def after_scenario(context, scenario):
	pass

# @fixture
# def setup_minifi_instance(context):
#     print("Setup", end="\n\n")
#     context.flow = None
#     context.test = MiNiFi_integration_test(context)

def before_all(context):
    context.config.setup_logging()

# def before_scenario(context, scenario):
# 	use_fixture(setup_minifi_instance, context)

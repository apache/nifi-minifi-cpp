from behave import fixture, use_fixture
import sys
sys.path.append('../minifi')
import logging

from MiNiFi_integration_test_driver import MiNiFi_integration_test
from minifi import *

def raise_exception(exception):
    raise exception

@fixture
def setup_minifi_instance(context):
    logging.info("Setup")
    context.flow = None
    context.test = MiNiFi_integration_test(context)

def before_scenario(context, scenario):
    use_fixture(setup_minifi_instance, context)

def after_scenario(context, scenario):
    context.test.cluster = None

# @fixture
# def setup_minifi_instance(context):
#     print("Setup", end="\n\n")
#     context.flow = None
#     context.test = MiNiFi_integration_test(context)

# def before_all(context):
#     print("Setup all", end="\n\n")
#     context.flow = None
#     context.test = MiNiFi_integration_test(context)

# def before_scenario(context, scenario):
# 	use_fixture(setup_minifi_instance, context)

from behave import fixture, use_fixture
import logging
import sys
sys.path.append('../minifi')

from MiNiFi_integration_test_driver import MiNiFi_integration_test  # noqa: E402
from minifi import *  # noqa


def raise_exception(exception):
    raise exception


@fixture
def test_driver_fixture(context):
    context.test = MiNiFi_integration_test(context)
    yield context.test
    logging.info("Integration test teardown...")
    del context.test


def before_scenario(context, scenario):
    use_fixture(test_driver_fixture, context)


def after_scenario(context, scenario):
    pass


def before_all(context):
    context.config.setup_logging()

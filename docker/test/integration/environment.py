import logging
import datetime
import sys
sys.path.append('../minifi')

from MiNiFi_integration_test_driver import MiNiFi_integration_test  # noqa: E402
from minifi import *  # noqa


def before_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        scenario.skip("Marked with @skip")
        return

    logging.info("Integration test setup at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test = MiNiFi_integration_test()


def after_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        logging.info("Scenario was skipped, no need for clean up.")
        return

    logging.info("Integration test teardown at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test.cleanup()


def before_all(context):
    context.config.setup_logging()

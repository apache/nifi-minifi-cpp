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


import logging
import datetime
import sys
sys.path.append('../minifi')

from MiNiFi_integration_test_driver import MiNiFi_integration_test  # noqa: E402
from minifi import *  # noqa
from minifi.core.ImageStore import ImageStore # noqa


def before_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        scenario.skip("Marked with @skip")
        return

    logging.info("Integration test setup at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test = MiNiFi_integration_test(context.image_store)


def after_scenario(context, scenario):
    if "skip" in scenario.effective_tags:
        logging.info("Scenario was skipped, no need for clean up.")
        return

    logging.info("Integration test teardown at {time:%H:%M:%S.%f}".format(time=datetime.datetime.now()))
    context.test.cleanup()


def before_all(context):
    context.config.setup_logging()
    context.image_store = ImageStore()

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

import platform
from minifi_test_framework.core.hooks import common_before_scenario
from minifi_test_framework.core.hooks import common_after_scenario

# These hooks are executed by behave before and after each scenario
# The common_before_scenario and common_after_scenario must be called for proper setup and tear down


def before_feature(context, feature):
    if "x86_x64_only" in feature.tags:
        is_x86 = platform.machine() in ("i386", "AMD64", "x86_64")
        if not is_x86:
            feature.skip("This feature is only x86/x64 compatible")


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)

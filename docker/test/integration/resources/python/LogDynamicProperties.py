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

def describe(processor):
    processor.setDescription("Test description.")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()
    processor.addProperty("Static Property", "Static Property")


def onTrigger(context, session):
    flow_file = session.get()
    property_value = context.getProperty("Static Property")
    log.info("Static Property value: {}".format(property_value))
    dyn_property_value = context.getDynamicProperty("Dynamic Property")
    log.info("Dynamic Property value: {}".format(dyn_property_value))
    keys = context.getDynamicPropertyKeys()
    log.info("dynamic property key count: {}".format(len(keys)))
    for dynamic_property_key in keys:
        log.info("dynamic property key: {}".format(dynamic_property_key))
    session.transfer(flow_file, REL_SUCCESS)

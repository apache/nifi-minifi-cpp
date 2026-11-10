# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


def describe(processor):
    processor.setDescription("Test description.")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()
    processor.addProperty("Static Property", "Static Property")


def onTrigger(context, session):
    flow_file = session.get()
    property_value = context.getProperty("Static Property")
    log.info(f"Static Property value: {property_value}")
    dyn_property_value = context.getDynamicProperty("Dynamic Property")
    log.info(f"Dynamic Property value: {dyn_property_value}")
    keys = context.getDynamicPropertyKeys()
    log.info(f"dynamic property key count: {len(keys)}")
    for dynamic_property_key in keys:
        log.info(f"dynamic property key: {dynamic_property_key}")
    session.transfer(flow_file, REL_SUCCESS)

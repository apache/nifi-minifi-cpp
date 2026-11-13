#
#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#
import bar
import foo


def describe(processor):
    processor.setDescription("Processor used for testing in ExecutePythonProcessorTests.cpp")


def onTrigger(context, session):
    flow_file = session.get()
    log.info(foo.foo() + bar.bar())

    if flow_file is not None:
        session.transfer(flow_file, REL_SUCCESS)

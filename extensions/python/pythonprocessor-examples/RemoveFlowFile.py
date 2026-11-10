# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


def describe(processor):
    processor.setDescription("Removes flow file from the session")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


def onTrigger(context, session):
    flow_file = session.get()
    if flow_file is not None:
        session.remove(flow_file)

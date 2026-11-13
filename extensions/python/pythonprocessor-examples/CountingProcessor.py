# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

i = 0


class WriteCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        output_stream.write(self.content.encode("utf-8"))
        return len(self.content)


def describe(processor):
    processor.setDescription("Counts up with the help of a global variable")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


def onTrigger(context, session):
    global i
    i = i + 1
    flow_file = session.create()
    session.write(flow_file, WriteCallback(str(i)))
    session.transfer(flow_file, REL_SUCCESS)

# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


def describe(processor):
    processor.setDescription("Processor used for testing in ExecutePythonProcessorTests.cpp")


state = 0


class WriteCallback:
    def process(self, output_stream):
        global state
        new_content = str(state).encode("utf-8")
        output_stream.write(new_content)
        state = state + 1
        return len(new_content)


def onTrigger(context, session):
    log.info("Vrrm, vrrrm, processor is running, vrrrm!!")
    # flow_file = session.get()
    flow_file = session.create()
    flow_file.setAttribute("filename", str(state))
    log.info("created flow file: {}".format(flow_file.getAttribute("filename")))

    if flow_file is not None:
        session.write(flow_file, WriteCallback())
        session.transfer(flow_file, REL_SUCCESS)

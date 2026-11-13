#
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#

import codecs
import time


class ReadCallback:
    def process(self, input_stream):
        self.content = codecs.getreader("utf-8")(input_stream).read()
        return len(self.content)


class WriteReverseStringCallback:
    def __init__(self, content):
        self.content = content

    def process(self, output_stream):
        reversed_content = self.content[::-1]
        output_stream.write(reversed_content.encode("utf-8"))
        return len(reversed_content)


def onTrigger(context, session):
    flow_file = session.get()
    if flow_file is not None:
        read_callback = ReadCallback()
        session.read(flow_file, read_callback)
        session.write(flow_file, WriteReverseStringCallback(read_callback.content))
        flow_file.addAttribute("python_timestamp", str(int(time.time())))
        session.transfer(flow_file, REL_SUCCESS)

#!/usr/bin/env python
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
from minifi import *

from argparse import ArgumentParser
from ctypes import cdll
import ctypes
import sys
from _cffi_backend import callback


class GetFilePrinterProcessor(PyProcessor):
    def __init__(self, minifi, flow):
        PyProcessor.__init__(self, minifi, flow)
        self._callback = None

    def _onTriggerCallback(self):
        def onTrigger(session, context):
            flow_file = self.get(session, context)
            if flow_file:
                if flow_file.add_attribute("python_test","value"):
                    print("Add attribute succeeded")
                if not flow_file.add_attribute("python_test","value2"):
                    print("Cannot add the same attribute twice!")
                print ("original file name: " + flow_file.get_attribute("filename"))
                self.transfer(session, flow_file, "success")
        return CALLBACK(onTrigger)


parser = ArgumentParser()
parser.add_argument("-s", "--dll", dest="dll_file",
                    help="DLL filename", metavar="FILE")

parser.add_argument("-n", "--nifi", dest="nifi_instance",
                    help="NiFi Instance")

parser.add_argument("-i", "--input", dest="input_port",
                    help="NiFi Input Port")

parser.add_argument("-d", "--dir", dest="dir",
                help="GetFile Dir to monitor", metavar="FILE")

args = parser.parse_args()

""" dll_file is the path to the shared object """
minifi = MiNiFi(dll_file=args.dll_file,url = args.nifi_instance.encode('utf-8'), port=args.input_port.encode('utf-8'))

minifi.set_property("nifi.remote.input.http.enabled","true")

processor = minifi.add_processor( GetFile() )

processor.set_property("Input Directory", args.dir)
processor.set_property("Keep Source File", "true")

current_module = sys.modules[__name__]

processor = minifi.create_python_processor(current_module,"GetFilePrinterProcessor")

ff = minifi.get_next_flowfile()
if ff:
    minifi.transmit_flowfile(ff)

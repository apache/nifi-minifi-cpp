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
"""
    Install the following with pip

    pip install datatable
    pip install pandas
"""
import codecs
import pandas as pd
import datatable as dt
from io import StringIO

def describe(processor):
    """ describe what this processor does
    """
    processor.setDescription("Converts the data source content of incoming flow file to csv. It \
                              supports a variety of data sources: pandas DataFrames, csv, numpy \
                              arrays, dictionary, list, raw Python objects, etc")

def onInitialize(processor):
    """ onInitialize is where you can set properties
    """
    processor.setSupportsDynamicProperties()

class ContentExtract(object):
    """ ContentExtract callback class is defined for reading streams of data through the session
        and has a process function that accepts the input stream
    """
    def __init__(self):
        self.content = None
    
    def process(self, input_stream):
        """ Use codecs getReader to read that data
        """
        self.content = codecs.getreader('utf-8')(input_stream).read()
        return len(self.content)

class ContentWrite(object):
    """ ContentWrite callback class is defined for writing streams of data through the session
    """
    def __init__(self, data):
        self.content = data

    def process(self, output_stream):
        """ Use codecs getWriter to write data encoded to the stream
        """
        codecs.getwriter('utf-8')(output_stream).write(self.content)
        return len(self.content)

def onTrigger(context, session):
    """ onTrigger is executed and passed processor context and session
    """
    flow_file = session.get()
    if flow_file is not None:
        read_cb = ContentExtract()
        # read flow file content into read_cb.content data member
        session.read(flow_file, read_cb)
        # create empty in-memory text streams csv_data buffer
        csv_data = StringIO()
        # load str data into datatable, then convert to pandas df
        dt_frame = dt.Frame(read_cb.content)
        pd_dframe = dt_frame.to_pandas() 
        # convert df to csv file like object without df index
        pd_dframe.to_csv(csv_data, index=False)
        # set the csv to the start of text stream
        csv_data.seek(0)
        # get csv str data out of StringIO text stream, then store str
        csv_data = csv_data.read()
        # write csv str to flow file
        write_cb = ContentWrite(csv_data)
        session.write(flow_file, write_cb)
        session.transfer(flow_file, REL_SUCCESS)
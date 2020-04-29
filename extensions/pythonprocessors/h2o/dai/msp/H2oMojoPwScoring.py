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

    -- after downloading the mojo scoring pipeline from Driverless AI,
       the following packages were needed for helping to make predictions

    pip install datatable pandas scipy

    -- after downloading the mojo2 py runtime from Driverless AI depending on
       your OS, you need to install the appropriate package:

    # Install the MOJO2 Py runtime on Mac OS X
    pip install path/to/daimojo-2.2.0-cp36-cp36m-macosx_10_7_x86_64.whl
    
    # Install the MOJO2 Py runtime on Linux x86
    pip install path/to/daimojo-2.2.0-cp36-cp36m-linux_x86_64.whl
    
    # Install the MOJO2 Py runtime on Linux PPC
    pip install path/to/daimojo-2.2.0-cp36-cp36m-linux_ppc64le.whl
"""
import codecs
import pandas as pd
import datatable as dt
from collections import Counter
from scipy.special._ufuncs import expit
import daimojo.model

def describe(processor):
    """ describe what this processor does
    """
    processor.setDescription("Executes H2O's MOJO Scoring Pipeline in C++ Runtime Python Wrapper \
        to do batch scoring or real time scoring for one or more predicted label(s) on the tabular \
        test data in the incoming flow file content. If tabular data is one row, then MOJO does \
        real time scoring. If tabular data is multiple rows, then MOJO does batch scoring.")

def onInitialize(processor):
    """ onInitialize is where you can set properties
    """
    processor.addProperty("MOJO Pipeline Filepath", "Add the filepath to the MOJO pipeline file. For example, \
        'path/to/mojo-pipeline/pipeline.mojo'.", "", True, False)

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
    # lambda compares two lists: does header equal expected header
    compare = lambda header, exp_header: Counter(header) == Counter(exp_header)
    if flow_file is not None:
        # read test data of flow file content into read_cb.content
        read_cb = ContentExtract()
        session.read(flow_file, read_cb)
        # instantiate H2O's MOJO Scoring Pipeline Scorer
        mojo_pipeline_filepath = context.getProperty("MOJO Pipeline Filepath")
        m_scorer = daimojo.model(mojo_pipeline_filepath)
        # add flow file attribute for creation time of mojo
        flow_file.addAttribute("mojo_creation_time", m_scorer.created_time)
        # add flow file attribute for uuid of mojo
        flow_file.addAttribute("mojo_uuid", m_scorer.uuid)
        # get list of predicted label(s) for prediction header
        pred_header = m_scorer.output_names
        # load tabular data str of 1 or more rows into datatable frame
        test_dt_frame = dt.Frame(read_cb.content)
        # does test dt frame column names (header) equal m_scorer feature_names (exp_header)
        if compare(test_dt_frame.names, m_scorer.feature_names) == False:
            test_dt_frame.names = tuple(m_scorer.feature_names)
        # do scoring on test data in the test_dt_frame, return dt frame with predicted label(s)
        preds_dt_frame = m_scorer.predict(test_dt_frame)
        # convert preds_dt_frame to pandas dataframe
        preds_df = preds_dt_frame.to_pandas()
        # convert pandas df to str without df index, then write to flow file
        preds_df_str = preds_df.to_string(index=False)
        write_cb = ContentWrite(preds_df_str)
        session.write(flow_file, write_cb)
        # add flow file attribute: number of rows to know how many rows were scored
        flow_file.addAttribute("num_rows_scored", str(preds_dt_frame.nrows))
        # add one or more flow file attributes: predicted label name and associated score pair
        for i in range(len(pred_header)):
            ff_attr_name = pred_header[i] + "_pred_0"
            flow_file.addAttribute(ff_attr_name, str(preds_df.at[0,pred_header[i]]))
            log.info("getAttribute({}): {}".format(ff_attr_name, flow_file.getAttribute(ff_attr_name)))
        session.transfer(flow_file, REL_SUCCESS)
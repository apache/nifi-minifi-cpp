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

    -- after downloading the python scoring pipeline from Driverless AI,
       the following was needed for executing the Scorer to make predictions
    pip install -r requirements.txt

        This requirements.txt file includes pip packages that are available on
        the internet for install, but there are some packages that only come with
        the python scoring pipeline download, which include:
    h2oaicore-1.8.4.1-cp36-cp36m-linux_x86_64.whl
    scoring_h2oai_experiment_6a77d0a4_6a25_11ea_becf_0242ac110002-1.0.0-py3-none-any.whl
"""
import codecs
import pandas as pd
import datatable as dt
from scipy.special._ufuncs import expit
from scoring_h2oai_experiment_6a77d0a4_6a25_11ea_becf_0242ac110002 import Scorer

def describe(processor):
    """ describe what this processor does
    """
    processor.setDescription("Executes H2O's Python Scoring Pipeline to do batch \
        scoring for one or more predicted label(s) on the tabular test data in the \
        incoming flow file content.")

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
        # read flow file tabular data content into read_cb.content data member
        session.read(flow_file, read_cb)
        # instantiate H2O's python scoring pipeline scorer
        scorer = Scorer()
        # load tabular data str into datatable
        test_dt_frame = dt.Frame(read_cb.content)
        # do batch scoring on test data in datatable frame, return pandas df with predicted labels
        batch_scores_df = scorer.score_batch(test_dt_frame)
        # convert df to str, then write to flow file
        batch_scores_df_str = batch_scores_df.to_string()
        write_cb = ContentWrite(batch_scores_df_str)
        session.write(flow_file, write_cb)
        # add flow file attribute: number of rows in the frame to know how many rows were scored
        flow_file.addAttribute("num_rows_scored", str(test_dt_frame.nrows))
        # add flow file attribute: the score of the first row in the frame to see it's pred label
        pred_header = batch_scores_df.columns
        for i in range(len(pred_header)):
            ff_attr_name = pred_header[i] + "_pred_0"
            flow_file.addAttribute(ff_attr_name, str(batch_scores_df.at[0,pred_header[i]]))
            log.info("getAttribute({}): {}".format(ff_attr_name, flow_file.getAttribute(ff_attr_name)))
        session.transfer(flow_file, REL_SUCCESS)
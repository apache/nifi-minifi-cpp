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
    processor.setDescription("Executes H2O's Python Scoring Pipeline to do real time scoring for \
        one or more predicted label(s) on the list test data in the incoming flow file content.")

def onInitialize(processor):
    """ onInitialize is where you can set properties
    """
    processor.addProperty("Predicted Label(s)", "Add One or more predicted label names for the prediction \
        header. If there is only one predicted label name, then write it in directly. If there is more than \
        one predicted label name, then write a comma separated list of predicted label names.", "", True, False)

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
        # read test data of flow file content into read_cb.content
        read_cb = ContentExtract()
        session.read(flow_file, read_cb)
        # instantiate H2O's Python Scoring Pipeline Scorer
        scorer = Scorer()
        # get predicted label(s) for prediction header: comma separated labels if more than one
        pred_header_str = context.getProperty("Predicted Label(s)")
        pred_header_list = pred_header_str.split(",")
        # load tabular data str into datatable, convert to pd df, then to list of lists
        test_dt_frame = dt.Frame(read_cb.content)
        test_pd_df = test_dt_frame.to_pandas()
        # grab first list since there is only 1 list in the list of lists 
        test_list = test_pd_df.values.tolist()[0]
        log.info("test_list = {}".format(test_list))
        log.info("len(test_list) = {}".format(len(test_list)))
        # do real time scoring on test data in the list, return list with predicted label(s)
        preds_list = scorer.score(test_list)
        # convert pred list to a comma-separated string followed by \n for line end
        preds_list_str = ','.join(map(str, preds_list)) + '\n'
        # concatenate prediction header and list string to pred table string
        preds_str = pred_header_str + '\n' + preds_list_str
        write_cb = ContentWrite(preds_str)
        session.write(flow_file, write_cb)
        # add flow file attribute: number of lists to know how many lists were scored
        flow_file.addAttribute("num_lists_scored", str(len(test_list)))
        # add one or more flow file attributes: predicted label name and associated score pair
        for i in range(len(pred_header_list)):
            ff_attr_name = pred_header_list[i] + "_pred"
            flow_file.addAttribute(ff_attr_name, str(preds_list[i]))
            log.info("getAttribute({}): {}".format(ff_attr_name, flow_file.getAttribute(ff_attr_name)))
        session.transfer(flow_file, REL_SUCCESS)
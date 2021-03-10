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
    -- after downloading the mojo model from h2o3, the following packages
       are needed to execute the model to do batch or real-time scoring

    Make all packages available on your machine:

    sudo apt-get -y update

    Install Java to include open source H2O-3 algorithms:

    sudo apt-get -y install openjdk-8-jdk

    Install Datatable and pandas:

    pip install datatable
    pip install pandas

    Option 1: Install H2O-3 with conda

    conda create -n h2o3-nifi-minifi python=3.6
    conda activate h2o3-nifi-minifi
    conda config --append channels conda-forge
    conda install -y -c h2oai h2o

    Option 2: Install H2O-3 with pip

    pip install requests
    pip install tabulate
    pip install "colorama>=0.3.8"
    pip install future
    pip uninstall h2o
    If on Mac OS X, must include --user:
        pip install -f http://h2o-release.s3.amazonaws.com/h2o/latest_stable_Py.html h2o --user
    else:
        pip install -f http://h2o-release.s3.amazonaws.com/h2o/latest_stable_Py.html h2o

"""
import h2o
import codecs
import pandas as pd  # noqa: F401
import datatable as dt

mojo_model = None


def describe(processor):
    """ describe what this processor does
    """
    processor.setDescription("Executes H2O-3's MOJO Model in Python to do batch scoring or \
        real-time scoring for one or more predicted label(s) on the tabular test data in \
        the incoming flow file content. If tabular data is one row, then MOJO does real-time \
        scoring. If tabular data is multiple rows, then MOJO does batch scoring.")


def onInitialize(processor):
    """ onInitialize is where you can set properties
        processor.addProperty(name, description, defaultValue, required, el)
    """
    processor.addProperty("MOJO Model Filepath", "Add the filepath to the MOJO Model file. For example, \
        'path/to/mojo-model/GBM_grid__1_AutoML_20200511_075150_model_180.zip'.", "", True, False)

    processor.addProperty("Is First Line Header", "Add True or False for whether first line is header.",
                          "True", True, False)

    processor.addProperty("Input Schema", "If first line is not header, then you must add Input Schema for \
        incoming data.If there is more than one column name, write a comma separated list of \
        column names. Else, you do not need to add an Input Schema.", "", False, False)

    processor.addProperty("Use Output Header", "Add True or False for whether you want to use an output \
        for your predictions.", "False", False, False)

    processor.addProperty("Output Schema", "To set Output Schema, 'Use Output Header' must be set to 'True' \
        If you want more descriptive column names for your predictions, then add an Output Schema. If there \
        is more than one column name, write a comma separated list of column names. Else, H2O-3 will include \
        them by default", "", False, False)


def onSchedule(context):
    """ onSchedule is where you load and read properties
        this function is called 1 time when the processor is scheduled to run
    """
    global mojo_model
    h2o.init()
    # instantiate H2O-3's MOJO Model
    mojo_model_filepath = context.getProperty("MOJO Model Filepath")
    mojo_model = h2o.import_mojo(mojo_model_filepath)


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
    global mojo_model
    flow_file = session.get()
    if flow_file is not None:
        # read test data of flow file content into read_cb.content
        read_cb = ContentExtract()
        session.read(flow_file, read_cb)
        # add flow file attribute for mojo model id
        # flow_file.addAttribute("mojo_model_id", mojo_model.model_id)
        # load tabular data str of 1 or more rows into datatable frame
        test_dt_frame = dt.Frame(read_cb.content)
        test_h2o_frame = h2o.H2OFrame(python_obj=test_dt_frame.to_numpy(), column_names=list(test_dt_frame.names))
        # does test dt frame column names (header) equal m_scorer feature_names (exp_header)
        first_line_header = context.getProperty("Is First Line Header")
        if first_line_header == "False":
            input_schema = context.getProperty("Input Schema")
            test_h2o_frame.names = list(input_schema.split(","))
        # do scoring on test data in the test_h2o_frame, return dt frame with predicted label(s)
        preds_h2o_frame = mojo_model.predict(test_h2o_frame)
        use_output_header = context.getProperty("Use Output Header")
        if use_output_header == "True":
            output_schema = context.getProperty("Output Schema")
            preds_h2o_frame.names = list(output_schema.split(","))
        # convert preds_h2o_frame to pandas dataframe, use_pandas=True by default
        preds_pd_df = h2o.as_list(preds_h2o_frame)
        # convert pandas df to str without df index, then write to flow file
        preds_pd_df_str = preds_pd_df.to_string(index=False)
        write_cb = ContentWrite(preds_pd_df_str)
        session.write(flow_file, write_cb)
        # get list of predicted label(s) for prediction header
        pred_header = preds_h2o_frame.names
        # add flow file attribute: number of rows to know how many rows were scored
        flow_file.addAttribute("num_rows_scored", str(preds_h2o_frame.nrows))
        # add one or more flow file attributes: predicted label name and associated score pair
        for i in range(len(pred_header)):
            ff_attr_name = pred_header[i] + "_pred_0"
            flow_file.addAttribute(ff_attr_name, str(preds_pd_df.at[0, pred_header[i]]))
            log.info("getAttribute({}): {}".format(ff_attr_name, flow_file.getAttribute(ff_attr_name)))
        session.transfer(flow_file, REL_SUCCESS)

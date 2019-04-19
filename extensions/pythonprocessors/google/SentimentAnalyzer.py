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
  Install the following with pip ( or pip3 )
  
  pip install google-cloud-language
  
  -- the following were needed during development as we saw SSL timeout errors
  pip install requests[security]
  pip install -U httplib2
"""
import json
import sys
import codecs
from google.cloud import language
from google.cloud.language import enums
from google.cloud.language import types

def describe(processor):
    processor.setDescription("Performs a sentiment Analysis of incoming flowfile content using Google Cloud.")

def onInitialize(processor):
    # is required, 
    processor.addProperty("Credentials Path","Path to your Google Credentials JSON File. Must exist on agent hosts.","", True, False)

class ContentExtract(object):
  def __init__(self):
    self.content = None

  def process(self, input_stream):
    self.content = codecs.getreader('utf-8')(input_stream).read()
    return len(self.content)


def onTrigger(context, session):
  flow_file = session.get()
  if flow_file is not None:
    credentials_filename = context.getProperty("Credentials Path")
    sentiment = ContentExtract()
    session.read(flow_file,sentiment)
    client = language.LanguageServiceClient.from_service_account_json(credentials_filename)
    document = types.Document(content=sentiment.content,type=enums.Document.Type.PLAIN_TEXT)
    
    annotations = client.analyze_sentiment(document=document, retry = None,timeout=1.0 )
    score = annotations.document_sentiment.score
    magnitude = annotations.document_sentiment.magnitude
    
    flow_file.addAttribute("score",str(score))
    flow_file.addAttribute("magnitude",str(magnitude))
    session.transfer(flow_file, REL_SUCCESS)


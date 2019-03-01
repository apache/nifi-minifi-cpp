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
import codecs
from vaderSentiment.vaderSentiment import SentimentIntensityAnalyzer

def describe(processor):
    processor.setDescription("Provides a sentiment analysis of the content within the flow file")

def onInitialize(processor):
  processor.setSupportsDynamicProperties()

class VaderSentiment(object):
  def __init__(self):
    self.content = None

  def process(self, input_stream):
    self.content = codecs.getreader('utf-8')(input_stream).read()
    return len(self.content)

def onTrigger(context, session):
  flow_file = session.get()
  if flow_file is not None:
    sentiment = VaderSentiment()
    session.read(flow_file,sentiment)
    analyzer = SentimentIntensityAnalyzer()
    vs = analyzer.polarity_scores(sentiment.content)
    flow_file.addAttribute("positive",str(vs['pos']))
    flow_file.addAttribute("negative",str(vs['neg']))
    flow_file.addAttribute("neutral",str(vs['neu']))
    session.transfer(flow_file, REL_SUCCESS)

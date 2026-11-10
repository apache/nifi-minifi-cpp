# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
import codecs

from vaderSentiment.vaderSentiment import SentimentIntensityAnalyzer


def describe(processor):
    processor.setDescription("Provides a sentiment analysis of the content within the flow file")


def onInitialize(processor):
    processor.setSupportsDynamicProperties()


class VaderSentiment:
    def __init__(self):
        self.content = None

    def process(self, input_stream):
        self.content = codecs.getreader("utf-8")(input_stream).read()
        return len(self.content)


def onTrigger(context, session):
    flow_file = session.get()
    if flow_file is not None:
        sentiment = VaderSentiment()
        session.read(flow_file, sentiment)
        analyzer = SentimentIntensityAnalyzer()
        vs = analyzer.polarity_scores(sentiment.content)
        flow_file.addAttribute("positive", str(vs["pos"]))
        flow_file.addAttribute("negative", str(vs["neg"]))
        flow_file.addAttribute("neutral", str(vs["neu"]))
        session.transfer(flow_file, REL_SUCCESS)

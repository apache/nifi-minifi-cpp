# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from containers.elasticsearch_container import ElasticsearchContainer
from containers.opensearch_container import OpensearchContainer
from minifi_behave.core.hooks import common_after_scenario, common_before_scenario

import docker


def before_feature(context, feature):
    if feature.filename.endswith("elasticsearch.feature"):
        docker.from_env().images.pull(ElasticsearchContainer.IMAGE)
    elif feature.filename.endswith("opensearch.feature"):
        docker.from_env().images.pull(OpensearchContainer.IMAGE)


def before_scenario(context, scenario):
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)

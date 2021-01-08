# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the \"License\"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an \"AS IS\" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import gzip
import logging
import tarfile
import uuid
import xml.etree.cElementTree as elementTree
from xml.etree.cElementTree import Element
from io import StringIO
from io import BytesIO
from textwrap import dedent

import docker
import os
import yaml
from copy import copy

import time
from collections import OrderedDict

from .core.Connectable import Connectable
from .core.Cluster import Cluster
from .core.Connectable import Connectable
from .core.ControllerService import ControllerService
from .core.InputPort import InputPort
from .core.Processor import Processor
from .core.RemoteProcessGroup import RemoteProcessGroup
from .core.SingleNodeDockerCluster import SingleNodeDockerCluster
from .core.SSLContextService import SSLContextService
from .core.DockerTestCluster import DockerTestCluster
from .core.OutputEventHandler import OutputEventHandler

from .flow_serialization.Minifi_flow_yaml_serializer import Minifi_flow_yaml_serializer
from .flow_serialization.Nifi_flow_xml_serializer import Nifi_flow_xml_serializer

from .processors.GenerateFlowFile import GenerateFlowFile
from .processors.GetFile import GetFile
from .processors.InvokeHTTP import InvokeHTTP
from .processors.ListenHTTP import ListenHTTP
from .processors.LogAttribute import LogAttribute
from .processors.PublishKafka import PublishKafka
from .processors.PublishKafkaSSL import PublishKafkaSSL
from .processors.PutFile import PutFile
from .processors.PutS3Object import PutS3Object
from .processors.DeleteS3Object import DeleteS3Object
from .processors.FetchS3Object import FetchS3Object

from .validators.OutputValidator import OutputValidator
from .validators.EmptyFilesOutPutValidator import EmptyFilesOutPutValidator
from .validators.SegfaultValidator import SegfaultValidator
from .validators.NoFileOutPutValidator import NoFileOutPutValidator
from .validators.SingleFileOutputValidator import SingleFileOutputValidator
from .validators.FileOutputValidator import FileOutputValidator

logging.basicConfig(level=logging.DEBUG)



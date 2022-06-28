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


class Cluster(object):
    """
    Base Cluster class. This is intended to be a generic interface
    to different types of clusters. Clusters could be Kubernetes clusters,
    Docker swarms, or cloud compute/container services.
    """

    def deploy_flow(self, name=None):
        """
        Deploys a flow to the cluster.
        """

    def stop_flow(self, container_name):
        """
        Stops a flow in the cluster.
        """

    def kill_flow(self, container_name):
        """
        Kills (ungracefully stops) a flow in the cluster.
        """

    def restart_flow(self, container_name):
        """
        Stops a flow in the cluster.
        """

    def __enter__(self):
        """
        Allocate ephemeral cluster resources.
        """
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Clean up ephemeral cluster resources.
        """

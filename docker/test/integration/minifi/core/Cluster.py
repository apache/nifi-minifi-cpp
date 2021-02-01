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

    def __enter__(self):
        """
        Allocate ephemeral cluster resources.
        """
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """
        Clean up ephemeral cluster resources.
        """

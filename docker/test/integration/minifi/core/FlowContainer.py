from .Container import Container


class FlowContainer(Container):
    def __init__(self, name, engine, vols, network):
        super().__init__(name, engine, vols, network)
        self.start_nodes = []

    def get_start_nodes(self):
        return self.start_nodes

    def add_start_node(self, node):
        self.start_nodes.append(node)

from .Container import Container


class FlowContainer(Container):
    def __init__(self, config_dir, name, engine, vols, network, image_store):
        super().__init__(name, engine, vols, network, image_store)
        self.start_nodes = []
        self.config_dir = config_dir

    def get_start_nodes(self):
        return self.start_nodes

    def add_start_node(self, node):
        self.start_nodes.append(node)

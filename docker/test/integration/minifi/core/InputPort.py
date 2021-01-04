from .Connectable import Connectable

class InputPort(Connectable):
    def __init__(self, name=None, remote_process_group=None):
        super(InputPort, self).__init__(name=name)

        self.remote_process_group = remote_process_group

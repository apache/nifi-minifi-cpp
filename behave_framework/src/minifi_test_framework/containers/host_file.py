class HostFile:
    def __init__(self, path, host_path):
        self.container_path = path
        self.host_path = host_path
        self.mode = "ro"

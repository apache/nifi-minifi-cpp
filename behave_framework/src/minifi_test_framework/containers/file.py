class File:
    def __init__(self, path, host_filename, content):
        self.path = path
        self.host_filename = host_filename
        self.content = content
        self.mode = "rw"

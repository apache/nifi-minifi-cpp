import uuid

class RemoteProcessGroup(object):
    def __init__(self, url, name=None):
        self.uuid = uuid.uuid4()

        if name is None:
            self.name = str(self.uuid)
        else:
            self.name = name

        self.url = url


    def get_name(self):
    	return self.name

    def get_uuid(self):
    	return self.uuid

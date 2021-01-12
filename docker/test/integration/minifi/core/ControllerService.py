import uuid
import logging

class ControllerService(object):
    def __init__(self, name=None, properties=None):

        self.id = str(uuid.uuid4())

        if name is None:
            self.name = str(uuid.uuid4())
            logging.info('Controller service name was not provided; using generated name \'%s\'', self.name)
        else:
            self.name = name

        if properties is None:
            properties = {}

        self.properties = properties

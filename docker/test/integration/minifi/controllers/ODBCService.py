from ..core.ControllerService import ControllerService


class ODBCService(ControllerService):
    def __init__(self, name=None, connection_string=None):
        super(ODBCService, self).__init__(name=name)

        self.service_class = 'ODBCService'

        if connection_string is not None:
            self.properties['Connection String'] = connection_string

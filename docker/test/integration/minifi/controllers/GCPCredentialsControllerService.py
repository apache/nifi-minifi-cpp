from ..core.ControllerService import ControllerService


class GCPCredentialsControllerService(ControllerService):
    def __init__(self, name=None, credentials_location=None, json_path=None, raw_json=None):
        super(GCPCredentialsControllerService, self).__init__(name=name)

        self.service_class = 'GCPCredentialsControllerService'

        if credentials_location is not None:
            self.properties['Credentials Location'] = credentials_location

        if json_path is not None:
            self.properties['Service Account JSON File'] = json_path

        if raw_json is not None:
            self.properties['Service Account JSON'] = raw_json

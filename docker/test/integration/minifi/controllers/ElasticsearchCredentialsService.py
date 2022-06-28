from ..core.ControllerService import ControllerService


class ElasticsearchCredentialsService(ControllerService):
    def __init__(self, api_key=None, name=None):
        super(ElasticsearchCredentialsService, self).__init__(name=name)

        self.service_class = 'ElasticsearchCredentialsControllerService'
        if api_key is None:
            self.properties['Username'] = "elastic"
            self.properties['Password'] = "password"
        else:
            self.properties['API Key'] = api_key

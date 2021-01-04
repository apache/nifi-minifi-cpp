from .ControllerService import ControllerService

class SSLContextService(ControllerService):
    def __init__(self, name=None, cert=None, key=None, ca_cert=None):
        super(SSLContextService, self).__init__(name=name)

        self.service_class = 'SSLContextService'

        if cert is not None:
            self.properties['Client Certificate'] = cert

        if key is not None:
            self.properties['Private Key'] = key

        if ca_cert is not None:
            self.properties['CA Certificate'] = ca_cert

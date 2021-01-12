from ..core.Processor import Processor

class InvokeHTTP(Processor):
    def __init__(self,
        ssl_context_service=None,
        schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
            properties = {
                "Proxy Host": "",
                "Proxy Port": "",
                "invokehttp-proxy-username": "",
                "invokehttp-proxy-password": "" }

            controller_services = []

            if ssl_context_service is not None:
                properties['SSL Context Service'] = ssl_context_service.name
                controller_services.append(ssl_context_service)

            super(InvokeHTTP, self).__init__('InvokeHTTP',
                properties = properties,
                controller_services = controller_services,
                auto_terminate = ['success', 'response', 'retry', 'failure', 'no retry'],
                schedule = schedule)
            self.out_proc.connect({"failure": self})

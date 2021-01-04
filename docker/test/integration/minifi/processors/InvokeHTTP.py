from ..core.Processor import Processor

class InvokeHTTP(Processor):
    def __init__(self, url,
        method='GET',
        proxy_host='',
        proxy_port='',
        proxy_username='',
        proxy_password='',
        ssl_context_service=None,
        schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
            properties = {
                'Remote URL': url,
                'HTTP Method': method,
                'Proxy Host': proxy_host,
                'Proxy Port': proxy_port,
                'invokehttp-proxy-username': proxy_username,
                'invokehttp-proxy-password': proxy_password }

            controller_services = []

            if ssl_context_service is not None:
                properties['SSL Context Service'] = ssl_context_service.name
                controller_services.append(ssl_context_service)

            super(InvokeHTTP, self).__init__('InvokeHTTP',
                properties = properties,
                controller_services = controller_services,
                auto_terminate = ['success', 'response', 'retry', 'failure', 'no retry'],
                schedule = schedule)

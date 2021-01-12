from ..core.Processor import Processor

class PutFile(Processor):
    def __init__(self, output_dir="/tmp/output", schedule={'scheduling strategy': 'EVENT_DRIVEN'}):
        super(PutFile, self).__init__('PutFile',
            properties={'Directory': output_dir, 'Directory Permissions': '777', 'Permissions': '777'},
            auto_terminate=['success', 'failure'],
            schedule=schedule)

    def nifi_property_key(self, key):
        if key == 'Directory Permissions':
            return None
        else:
            return key

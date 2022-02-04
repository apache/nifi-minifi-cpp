from .MinifiContainer import MinifiContainer


class TransientMinifiContainer(MinifiContainer):
    def __init__(self, config_dir, name, vols, network, image_store, command=None):
        if not command:
            command = ["/bin/sh", "-c", "cp /tmp/minifi_config/config.yml " + MinifiContainer.MINIFI_ROOT + "/conf && /opt/minifi/minifi-current/bin/minifi.sh start && sleep 10 && /opt/minifi/minifi-current/bin/minifi.sh stop && sleep 100"]
        super().__init__(config_dir, name, vols, network, image_store, command)

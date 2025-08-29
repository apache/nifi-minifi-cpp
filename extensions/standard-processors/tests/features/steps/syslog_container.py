from minifi_test_framework.containers.container import Container


class SyslogContainer(Container):
    def __init__(self, protocol, context):
        super(SyslogContainer, self).__init__("ubuntu:24.04", f"syslog-{protocol}-{context.scenario_id}", context.network)
        self.command = f'/bin/bash -c "echo Syslog {protocol} client started; while true; do logger --{protocol} -n minifi-{context.scenario_id} -P 514 sample_log; sleep 1; done"'

from typing import Protocol


class MinifiProtocol(Protocol):
    def set_property(self, key: str, value: str):
        ...

    def set_log_property(self, key: str, value: str):
        ...

    def set_deploy_timeout_seconds(self, timeout_seconds: int):
        ...


def set_controller_socket_properties(minifi: MinifiProtocol):
    minifi.set_property("controller.socket.enable", "true")
    minifi.set_property("controller.socket.host", "localhost")
    minifi.set_property("controller.socket.port", "9998")
    minifi.set_property("controller.socket.local.any.interface", "false")


def enable_openssl_fips_mode(minifi: MinifiProtocol):
    minifi.set_property("nifi.openssl.fips.support.enable", "true")


def enable_log_metrics_publisher(minifi: MinifiProtocol):
    minifi.set_property("nifi.metrics.publisher.LogMetricsPublisher.metrics", "RepositoryMetrics")
    minifi.set_property("nifi.metrics.publisher.LogMetricsPublisher.logging.interval", "1s")
    minifi.set_property("nifi.metrics.publisher.class", "LogMetricsPublisher")


def configure_c2_flow_url(minifi: MinifiProtocol, scenario_id: str):
    minifi.set_property("nifi.c2.flow.url", f"http://minifi-c2-server-{scenario_id}:10090/c2/config?class=minifi-test-class")


def set_up_ssl_properties(minifi: MinifiProtocol):
    minifi.set_property("nifi.remote.input.secure", "true")
    minifi.set_property("nifi.security.client.certificate", "/tmp/resources/minifi_client.crt")
    minifi.set_property("nifi.security.client.private.key", "/tmp/resources/minifi_client.key")
    minifi.set_property("nifi.security.client.ca.certificate", "/tmp/resources/root_ca.crt")

import logging
from .Container import Container
from textwrap import dedent


class HttpProxyContainer(Container):
    def __init__(self, name, vols, network):
        super().__init__(name, 'http-proxy', vols, network)

    def get_startup_finish_text(self):
        return "Accepting HTTP Socket connections at"

    def deploy(self):
        if not self.set_deployed():
            return

        logging.info('Creating and running http-proxy docker container...')
        dockerfile = dedent("""FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid3/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                ENTRYPOINT ["/sbin/entrypoint.sh"]
                """.format(base_image='sameersbn/squid:3.5.27-2', proxy_username='admin', proxy_password='test101', proxy_port='3128'))
        configured_image = self.build_image(dockerfile, [])
        self.client.containers.run(
            configured_image[0],
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={'3128/tcp': 3128})
        logging.info('Added container \'%s\'', self.name)

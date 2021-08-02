from .Container import Container
from textwrap import dedent


class PostgreSQLServerContainer(Container):
    def __init__(self, name, vols, network):
        super().__init__(name, 'postgresql-server', vols, network)

    def get_startup_finished_log_entry(self):
        return "database system is ready to accept connections"

    def deploy(self):
        if not self.set_deployed():
            return

        dockerfile = dedent("""FROM {base_image}
                RUN mkdir -p /docker-entrypoint-initdb.d
                RUN echo "#!/bin/bash" > /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "set -e" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "psql -v ON_ERROR_STOP=1 --username "postgres" --dbname "postgres" <<-EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table (int_col INTEGER, text_col TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (1, 'apple');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (2, 'banana');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (3, 'pear');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh
                """.format(base_image='postgres:13.2'))
        configured_image = self.build_image(dockerfile, [])
        self.docker_container = self.client.containers.run(
            configured_image[0],
            detach=True,
            name='postgresql-server',
            network=self.network.name,
            environment=["POSTGRES_PASSWORD=password"])

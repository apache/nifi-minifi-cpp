import time


class PostgresChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    def __query_postgres_server(self, postgresql_container_name, query, number_of_rows):
        (code, output) = self.container_communicator.execute_command(postgresql_container_name, ["psql", "-U", "postgres", "-c", query])
        return code == 0 and str(number_of_rows) + " rows" in output

    def check_query_results(self, postgresql_container_name, query, number_of_rows, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.__query_postgres_server(postgresql_container_name, query, number_of_rows):
                return True
            time.sleep(2)
        return False

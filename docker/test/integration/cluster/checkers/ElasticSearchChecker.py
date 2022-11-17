import json


class ElasticSearchChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    def is_elasticsearch_empty(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["curl", "-u", "elastic:password", "-k", "-XGET", "https://localhost:9200/_search"])
        return code == 0 and '"hits":[]' in output

    def create_doc_elasticsearch(self, container_name, index_name, doc_id):
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c",
                                                                                      "curl -u elastic:password -k -XPUT https://localhost:9200/" + index_name + "/_doc/" + doc_id + " -H Content-Type:application/json -d'{\"field1\":\"value1\"}'"])
        return code == 0 and ('"_id":"' + doc_id + '"').encode() in output

    def check_elastic_field_value(self, container_name, index_name, doc_id, field_name, field_value):
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c",
                                                                                      "curl -u elastic:password -k -XGET https://localhost:9200/" + index_name + "/_doc/" + doc_id])
        return code == 0 and (field_name + '":"' + field_value).encode() in output

    def elastic_generate_apikey(self, elastic_container_name):
        (_, output) = self.container_communicator.execute_command(elastic_container_name, ["/bin/bash", "-c",
                                                                                           "curl -u elastic:password -k -XPOST https://localhost:9200/_security/api_key -H Content-Type:application/json -d'{\"name\":\"my-api-key\",\"expiration\":\"1d\",\"role_descriptors\":{\"role-a\": {\"cluster\": [\"all\"],\"index\": [{\"names\": [\"my_index\"],\"privileges\": [\"all\"]}]}}}'"])
        output_lines = output.splitlines()
        result = json.loads(output_lines[-1])
        return result["encoded"]

    def add_elastic_user_to_opensearch(self, container_name):
        (code, output) = self.container_communicator.execute_command(container_name, ["/bin/bash", "-c",
                                                                                      'curl -u admin:admin -k -XPUT https://opensearch:9200/_plugins/_security/api/internalusers/elastic -H Content-Type:application/json -d\'{"password":"password","backend_roles":["admin"]}\''])
        return code == 0 and '"status":"CREATED"'.encode() in output

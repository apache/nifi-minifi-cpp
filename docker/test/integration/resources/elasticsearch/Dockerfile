FROM elasticsearch:8.2.2
COPY elasticsearch.yml /usr/share/elasticsearch/config/elasticsearch.yml
COPY certs/elastic_http.key /usr/share/elasticsearch/config/certs/elastic_http.key
COPY certs/elastic_http.crt /usr/share/elasticsearch/config/certs/elastic_http.crt
COPY certs/elastic_transport.key /usr/share/elasticsearch/config/certs/elastic_transport.key
COPY certs/elastic_transport.crt /usr/share/elasticsearch/config/certs/elastic_transport.crt
COPY certs/root_ca.crt /usr/share/elasticsearch/config/certs/root_ca.crt

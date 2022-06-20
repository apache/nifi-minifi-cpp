FROM elasticsearch:8.2.2
COPY elasticsearch.yml /usr/share/elasticsearch/config/elasticsearch.yml
COPY certs/elastic_cert.key /usr/share/elasticsearch/config/certs/elastic_cert.key
COPY certs/elastic_cert.pem /usr/share/elasticsearch/config/certs/elastic_cert.pem
COPY certs/elastic_transport.key /usr/share/elasticsearch/config/certs/elastic_transport.key
COPY certs/elastic_transport.pem /usr/share/elasticsearch/config/certs/elastic_transport.pem
COPY certs/root_ca.pem /usr/share/elasticsearch/config/certs/root_ca.pem

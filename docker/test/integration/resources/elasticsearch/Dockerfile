FROM elasticsearch:8.2.2
ADD elasticsearch.yml /usr/share/elasticsearch/config/elasticsearch.yml
ADD certs/elastic_cert.key /usr/share/elasticsearch/config/certs/elastic_cert.key
ADD certs/elastic_cert.pem /usr/share/elasticsearch/config/certs/elastic_cert.pem
ADD certs/elastic_transport.key /usr/share/elasticsearch/config/certs/elastic_transport.key
ADD certs/elastic_transport.pem /usr/share/elasticsearch/config/certs/elastic_transport.pem
ADD certs/root_ca.pem /usr/share/elasticsearch/config/certs/root_ca.pem

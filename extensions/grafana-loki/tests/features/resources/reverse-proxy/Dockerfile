# Source: https://gist.github.com/laurentbel/c4c7696890fc71c8061172a932eb52e4
FROM nginx:1.25.3

RUN apt-get update -y && apt-get install -y apache2-utils && rm -rf /var/lib/apt/lists/*

WORKDIR /
COPY nginx-basic-auth.conf nginx-basic-auth.conf

COPY run.sh ./
RUN chmod 0755 ./run.sh
CMD [ "./run.sh" ]

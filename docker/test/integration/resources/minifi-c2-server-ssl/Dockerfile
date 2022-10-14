FROM apache/nifi-minifi-c2:1.18.0
RUN mkdir $MINIFI_C2_HOME/certs/
COPY --chown=c2:c2 certs/* $MINIFI_C2_HOME/certs/
COPY --chown=c2:c2 config.yml $MINIFI_C2_HOME/files/minifi-test-class/config.text.yml.v1
COPY --chown=c2:c2 authorities.yaml $MINIFI_C2_HOME/conf/
COPY --chown=c2:c2 authorizations.yaml $MINIFI_C2_HOME/conf/
COPY --chown=c2:c2 c2.properties $MINIFI_C2_HOME/conf/

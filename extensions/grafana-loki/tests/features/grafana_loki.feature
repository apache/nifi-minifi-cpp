# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@ENABLE_GRAFANA_LOKI
Feature: MiNiFi can publish logs to Grafana Loki server

  Scenario: Logs are published to Loki server through REST API
    Given a Grafana Loki server is set up
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiREST processor with the "Url" property set to "http://grafana-loki-server-${scenario_id}:3100/"
    And PushGrafanaLokiREST is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiREST processor is set to "job=minifi,id=docker-test"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiREST
    And PushGrafanaLokiREST's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server to a specific tenant through REST API
    Given a Grafana Loki server is set up with multi-tenancy enabled
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiREST processor with the "Url" property set to "http://grafana-loki-server-${scenario_id}:3100/"
    And PushGrafanaLokiREST is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiREST processor is set to "job=minifi,id=docker-test"
    And the "Tenant ID" property of the PushGrafanaLokiREST processor is set to "mytenant"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiREST
    And PushGrafanaLokiREST's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the "mytenant" tenant on the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server through REST API using SSL
    Given a Grafana Loki server with SSL is set up
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiREST processor with the "Url" property set to "https://grafana-loki-server-${scenario_id}:3100/"
    And PushGrafanaLokiREST is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiREST processor is set to "job=minifi,id=docker-test"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiREST
    And the "SSL Context Service" property of the PushGrafanaLokiREST processor is set to "SSLContextService"
    And an ssl context service is set up
    And PushGrafanaLokiREST's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published using SSL to the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server with basic authentication through REST API using a reverse proxy
    Given a Grafana Loki server is set up
    And a reverse proxy is set up to forward requests to the Grafana Loki server
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiREST processor with the "Url" property set to "http://reverse-proxy-${scenario_id}:3030/"
    And PushGrafanaLokiREST is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiREST processor is set to "job=minifi,id=docker-test"
    And the "Username" property of the PushGrafanaLokiREST processor is set to "admin"
    And the "Password" property of the PushGrafanaLokiREST processor is set to "password"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiREST
    And PushGrafanaLokiREST's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server through gRPC
    Given a Grafana Loki server is set up
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiGrpc processor with the "Url" property set to "grafana-loki-server-${scenario_id}:9095"
    And PushGrafanaLokiGrpc is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiGrpc processor is set to "job=minifi,id=docker-test"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiGrpc
    And PushGrafanaLokiGrpc's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server to a specific tenant through gRPC
    Given a Grafana Loki server is set up with multi-tenancy enabled
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiGrpc processor with the "Url" property set to "grafana-loki-server-${scenario_id}:9095"
    And PushGrafanaLokiGrpc is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiGrpc processor is set to "job=minifi,id=docker-test"
    And the "Tenant ID" property of the PushGrafanaLokiGrpc processor is set to "mytenant"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiGrpc
    And PushGrafanaLokiGrpc's success relationship is auto-terminated

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published to the "mytenant" tenant on the Grafana Loki server in less than 60 seconds

  Scenario: Logs are published to Loki server through gRPC using SSL
    Given a Grafana Loki server with SSL is set up
    And a TailFile processor with the "File to Tail" property set to "/tmp/input/test_file.log"
    And a file with filename "test_file.log" and content "log line 1\nlog line 2\nlog line 3\n" is present in "/tmp/input"
    And a PushGrafanaLokiGrpc processor with the "Url" property set to "grafana-loki-server-${scenario_id}:9095"
    And PushGrafanaLokiGrpc is EVENT_DRIVEN
    And the "Stream Labels" property of the PushGrafanaLokiGrpc processor is set to "job=minifi,id=docker-test"
    And the "SSL Context Service" property of the PushGrafanaLokiGrpc processor is set to "SSLContextService"
    And the "success" relationship of the TailFile processor is connected to the PushGrafanaLokiGrpc
    And PushGrafanaLokiGrpc's success relationship is auto-terminated
    And an ssl context service is set up

    When all instances start up
    Then "log line 1;log line 2;log line 3" lines are published using SSL to the Grafana Loki server in less than 60 seconds

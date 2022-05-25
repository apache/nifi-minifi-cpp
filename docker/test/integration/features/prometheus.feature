Feature: MiNiFi can publish metrics to Prometheus server

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Published metrics are scraped by Prometheus server
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And a Prometheus server is set up
    When all instances start up
    Then "RepositoryMetrics" is published to the Prometheus server in less than 60 seconds
    And "QueueMetrics" is published to the Prometheus server in less than 60 seconds
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "FlowInformation" is published to the Prometheus server in less than 60 seconds
    And "DeviceInfoNode" is published to the Prometheus server in less than 60 seconds

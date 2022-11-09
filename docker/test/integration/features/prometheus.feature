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
    And "PutFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "PutFile" processor
    And "FlowInformation" is published to the Prometheus server in less than 60 seconds
    And "DeviceInfoNode" is published to the Prometheus server in less than 60 seconds
    And "AgentStatus" is published to the Prometheus server in less than 60 seconds

  Scenario: Multiple GetFile metrics are reported by Prometheus
    Given a GetFile processor with the name "GetFile1" and the "Input Directory" property set to "/tmp/input"
    And a GetFile processor with the name "GetFile2" and the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile1 processor is set to "true"
    And the "Keep Source File" property of the GetFile2 processor is set to "true"
    And "GetFile2" processor is a start node
    And a file with the content "test" is present in "/tmp/input"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile1 processor is connected to the PutFile
    And the "success" relationship of the GetFile2 processor is connected to the PutFile
    And a Prometheus server is set up
    When all instances start up
    Then "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile1" processor
    And "GetFileMetrics" processor metric is published to the Prometheus server in less than 60 seconds for "GetFile2" processor

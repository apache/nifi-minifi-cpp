Feature: Sending data to Splunk HEC using PutSplunkHTTP

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to a Splunk HEC
    Given a Splunk HEC is set up and running
    And a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "foobar" is present in "/tmp/input"
    And a PutSplunkHTTP processor set up to communicate with the Splunk HEC instance
    And a QuerySplunkIndexingStatus processor set up to communicate with the Splunk HEC Instance
    And the "Splunk Request Channel" properties of the PutSplunkHTTP and QuerySplunkIndexingStatus processors are set to the same random guid
    And the "Source" property of the PutSplunkHTTP processor is set to "my-source"
    And the "Source Type" property of the PutSplunkHTTP processor is set to "my-source-type"
    And the "Host" property of the PutSplunkHTTP processor is set to "my-host"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutSplunkHTTP
    And the "success" relationship of the PutSplunkHTTP processor is connected to the QuerySplunkIndexingStatus
    And the "undetermined" relationship of the QuerySplunkIndexingStatus processor is connected to the QuerySplunkIndexingStatus
    And the "acknowledged" relationship of the QuerySplunkIndexingStatus processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "foobar" is placed in the monitored directory in less than 20 seconds
    And an event is registered in Splunk HEC with the content "foobar" with "my-source" set as source and "my-source-type" set as sourcetype and "my-host" set as host

Feature: Sending data to Google Cloud Storage using PutGcsObject

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can upload data to Google Cloud storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "hello_gcs" is present in "/tmp/input"
    And a Google Cloud storage server is set up
    And a PutGcsObject processor
    And PutGcsObject processor is set up with a GcpCredentialsControllerService to communicate with the Google Cloud storage server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutGcsObject
    And the "success" relationship of the PutGcsObject processor is connected to the PutFile

    When all instances start up

    Then a flowfile with the content "hello_gcs" is placed in the monitored directory in less than 45 seconds
    And object with the content "hello_gcs" is present in the Google Cloud storage

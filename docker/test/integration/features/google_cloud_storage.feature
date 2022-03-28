Feature: Sending data to Google Cloud Storage using PutGCSObject

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can upload data to Google Cloud storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "hello_gcs" is present in "/tmp/input"
    And a Google Cloud storage server is set up
    And a PutGCSObject processor
    And the PutGCSObject processor is set up with a GCPCredentialsControllerService to communicate with the Google Cloud storage server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutGCSObject
    And the "success" relationship of the PutGCSObject processor is connected to the PutFile

    When all instances start up

    Then a flowfile with the content "hello_gcs" is placed in the monitored directory in less than 45 seconds
    And an object with the content "hello_gcs" is present in the Google Cloud storage

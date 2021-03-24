Feature: Sending data from MiNiFi-C++ to an Azure storage server
  In order to transfer data to interact with Azure servers
  As a user of MiNiFi
  I need to have a PutAzureBlobStorage processor

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance can upload data to Azure blob storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "#test_data$123$#" is present in "/tmp/input"
    And a PutAzureBlobStorage processor set up to communicate with an Azure blob storage
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutAzureBlobStorage
    And the "success" relationship of the PutAzureBlobStorage processor is connected to the PutFile

    And an Azure storage server "azure-storage" is set up in correspondence with the PutAzureBlobStorage

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the object on the "azure-storage" Azure storage server is "#test_data$123$#"

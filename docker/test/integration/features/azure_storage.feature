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

    And an Azure storage server is set up

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the object on the Azure storage server is "#test_data$123$#"

  Scenario: A MiNiFi instance can delete blob from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage

    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob from Azure blob storage including snapshots
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Include Snapshots"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage with a snapshot

    Then the Azure blob storage becomes empty in 30 seconds

  Scenario: A MiNiFi instance can delete blob snapshots from Azure blob storage
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And a DeleteAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the DeleteAzureBlobStorage processor is set to "test"
    And the "Delete Snapshots Option" property of the DeleteAzureBlobStorage processor is set to "Delete Snapshots Only"
    And the "success" relationship of the GenerateFlowFile processor is connected to the DeleteAzureBlobStorage
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" is created on Azure blob storage with a snapshot

    Then the blob and snapshot count becomes 1 in 30 seconds

  Scenario: A MiNiFi instance can fetch a blob from Azure blob storage
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" property of the GetFile processor is set to "true"
    And a file with the content "dummy" is present in "/tmp/input"
    And a FetchAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Blob" property of the FetchAzureBlobStorage processor is set to "test"
    And the "Range Start" property of the FetchAzureBlobStorage processor is set to "6"
    And the "Range Length" property of the FetchAzureBlobStorage processor is set to "5"
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the FetchAzureBlobStorage
    And the "success" relationship of the FetchAzureBlobStorage processor is connected to the PutFile
    And an Azure storage server is set up

    When all instances start up
    And test blob "test" with the content "#test_data$123$#" is created on Azure blob storage

    Then a flowfile with the content "data$" is placed in the monitored directory in less than 60 seconds

  Scenario: A MiNiFi instance can list a container on Azure blob storage
    Given a ListAzureBlobStorage processor set up to communicate with an Azure blob storage
    And the "Prefix" property of the ListAzureBlobStorage processor is set to "test"
    And a LogAttribute processor with the "FlowFiles To Log" property set to "0"
    And the "success" relationship of the ListAzureBlobStorage processor is connected to the LogAttribute
    And an Azure storage server is set up

    When all instances start up
    And test blob "test_1" with the content "data_1" is created on Azure blob storage
    And test blob "test_2" with the content "data_2" is created on Azure blob storage
    And test blob "other_test" with the content "data_3" is created on Azure blob storage

    Then the Minifi logs contain the following message: "key:azure.blobname value:test_1" in less than 60 seconds
    Then the Minifi logs contain the following message: "key:azure.blobname value:test_2" in less than 60 seconds
    And the Minifi logs do not contain the following message: "key:azure.blobname value:other_test" after 0 seconds

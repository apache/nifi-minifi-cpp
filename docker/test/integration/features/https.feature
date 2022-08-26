Feature: Using SSL context service to send data with TLS
  In order to send data via HTTPS
  As a user of MiNiFi
  I need to have access to the SSLContextService

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance sends data using InvokeHTTP to a receiver using ListenHTTP with TLS
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And 200 files with the content "test" are present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "https://secondary:4430/contentListener"
    And the "HTTP Method" property of the InvokeHTTP processor is set to "POST"

    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "4430" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    And an ssl context service set up for InvokeHTTP and ListenHTTP
    When both instances start up
    Then 200 flowfiles with the content "test" are placed in the monitored directory in less than 60 seconds

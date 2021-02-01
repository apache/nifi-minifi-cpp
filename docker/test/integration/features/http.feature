Feature: Verify sending using InvokeHTTP to a receiver using ListenHTTP
    In order to send and receive data via HTTP
    As a user of MiNiFi
    I need to have ListenHTTP and InvokeHTTP processors

    Background:
      Given the content of "/tmp/output" is monitored

    Scenario: A MiNiFi instance and transfers data to another MiNiFi instance
      Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
      And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
      And the "HTTP Method" of the InvokeHTTP processor is set to "POST"
      And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

      And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
      And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
      And the "success" relationship of the ListenHTTP processor is connected to the PutFile

      When both instances start up
      Then flowfiles are placed in the monitored directory in less than 30 seconds

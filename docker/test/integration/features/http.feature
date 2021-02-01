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


  Scenario: A MiNiFi instance sends data to a HTTP proxy and another one listens
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://minifi-listen:8080/contentListener"
    And these processor properties are set:
      | processor name | property name             | property value |
      | InvokeHTTP     | HTTP Method               | POST           |
      | InvokeHTTP     | Proxy Host                | http-proxy     |
      | InvokeHTTP     | Proxy Port                | 3128           |
      | InvokeHTTP     | invokehttp-proxy-username | admin          |
      | InvokeHTTP     | invokehttp-proxy-password | test101        |
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a http proxy server "http-proxy" is set up accordingly 

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "minifi-listen" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "minifi-listen" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When all instances start up
    Then flowfiles are placed in the monitored directory in less than 120 seconds
    And no errors were generated on the "http-proxy" regarding "http://minifi-listen:8080/contentListener"

Feature: Sending data using InvokeHTTP to a receiver using ListenHTTP
  In order to send and receive data via HTTP
  As a user of MiNiFi
  I need to have ListenHTTP and InvokeHTTP processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 30 seconds

  Scenario: Multiple files transfered via HTTP are received and transferred only once
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Keep Source File" of the GetFile processor is set to "false"
    And two files with content "first message" and "second message" are placed in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then two flowfiles with the contents "first message" and "second message" are placed in the monitored directory in less than 60 seconds

  Scenario: A MiNiFi instance sends data through a HTTP proxy and another one listens
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://minifi-listen:8080/contentListener"
    And these processor properties are set to match the http proxy:
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
    Then a flowfile with the content "test" is placed in the monitored directory in less than 120 seconds
    And no errors were generated on the "http-proxy" regarding "http://minifi-listen:8080/contentListener"

  Scenario: A MiNiFi instance and transfers hashed data to another MiNiFi instance
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a HashContent processor with the "Hash Attribute" property set to "hash"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" of the InvokeHTTP processor is set to "POST"
    And the "success" relationship of the GetFile processor is connected to the HashContent
    And the "success" relationship of the HashContent processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then a flowfile with the content "test" is placed in the monitored directory in less than 30 seconds

  Scenario: A MiNiFi instance transfers data to another MiNiFi instance without message body
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a InvokeHTTP processor with the "Remote URL" property set to "http://secondary:8080/contentListener"
    And the "HTTP Method" of the InvokeHTTP processor is set to "POST"
    And the "Send Message Body" of the InvokeHTTP processor is set to "false"
    And the "success" relationship of the GetFile processor is connected to the InvokeHTTP

    And a ListenHTTP processor with the "Listening Port" property set to "8080" in a "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output" in the "secondary" flow
    And the "success" relationship of the ListenHTTP processor is connected to the PutFile

    When both instances start up
    Then at least one empty flowfile is placed in the monitored directory in less than 30 seconds

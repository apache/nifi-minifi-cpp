@SUPPORTS_WINDOWS
Feature: Testing custom and default metrics

  Scenario: CustomMetrics(GetFileRs), DefaultMetrics from streaming(DuplicateStreamText) API and DefaultMetrics from buffer(PutFileRs) API
    Given a GetFileRs processor with the "Input Directory" property set to "/tmp/input"
    And a DuplicateStreamText processor
    And a PutFileRs processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFileRs processor is connected to the DuplicateStreamText
    And the "success" relationship of the DuplicateStreamText processor is connected to the PutFileRs
    And PutFileRs's success relationship is auto-terminated
    And PutFileRs's failure relationship is auto-terminated
    And a directory at "/tmp/input" has a file "hello.txt" with the content "hello"
    And MiNiFi logs processor metrics

    When the MiNiFi instance starts up

    Then at least one file with the content "hheelllloo" is placed in the "/tmp/output" directory in less than 10 seconds
    And the Minifi logs match the following regex: "GetFileRsMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "0",[\n ]*"BytesWritten": "5"[ a-zA-Z0-9":,\n]*"InputBytes": "5"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds
    And the Minifi logs match the following regex: "DuplicateStreamTextMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "5",[\n ]*"BytesWritten": "10"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds
    And the Minifi logs match the following regex: "PutFileRsMetrics": {\n[ ]+\"[0-9a-z-]+\": \{\n[ a-zA-Z0-9":,\n]*"BytesRead": "10"[ a-zA-Z0-9":,\n]*}" in less than 10 seconds

    And the Minifi logs do not contain errors
    And the Minifi logs do not contain warnings

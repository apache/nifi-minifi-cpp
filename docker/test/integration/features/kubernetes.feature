Feature: TailFile can collect logs from Kubernetes pods

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: Collect all logs from the default namespace
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then two flowfiles with the contents "Hello World!" and "Hello again, World!" are placed in the monitored directory in less than 30 seconds

  Scenario: Collect logs from selected pods
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service with the "Pod Name Filter" property set to ".*one"
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then one flowfile with the contents "Hello World!" is placed in the monitored directory in less than 30 seconds

  Scenario: Collect logs from selected containers
    Given a TailFile processor in a Kubernetes cluster
    And the "tail-mode" property of the TailFile processor is set to "Multiple file"
    And the "tail-base-directory" property of the TailFile processor is set to "/var/log/pods/${namespace}_${pod}_${uid}/${container}"
    And the "File to Tail" property of the TailFile processor is set to ".*\.log"
    And the "Lookup frequency" property of the TailFile processor is set to "1s"
    And the TailFile processor has an Attribute Provider Service which is a Kubernetes Controller Service with the "Container Name Filter" property set to "echo-[^o].."
    And a PutFile processor in the Kubernetes cluster
    And the "Directory" property of the PutFile processor is set to "/tmp/output"
    And the "success" relationship of the TailFile processor is connected to the PutFile
    When the MiNiFi instance starts up
    Then one flowfile with the contents "Hello again, World!" is placed in the monitored directory in less than 30 seconds

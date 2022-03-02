# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

Feature: Sending data from MiNiFi-C++ to an AWS server
  In order to transfer data to interact with AWS servers
  As a user of MiNiFi
  I need to have PutS3Object and DeleteS3Object processors

  Background:
    Given the content of "/tmp/output" is monitored

  Scenario: A MiNiFi instance transfers encoded data to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And the "success" relationship of the PutS3Object processor is connected to the PutFile

    And a s3 server is set up in correspondence with the PutS3Object

    When both instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 60 seconds
    And the object on the s3 server is "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And the object content type on the s3 server is "application/octet-stream" and the object metadata matches use metadata

  Scenario: A MiNiFi instance transfers encoded data through a http proxy to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And these processor properties are set to match the http proxy:
    | processor name  | property name  | property value |
    | PutS3Object     | Proxy Host     | http-proxy     |
    | PutS3Object     | Proxy Port     | 3128           |
    | PutS3Object     | Proxy Username | admin          |
    | PutS3Object     | Proxy Password | test101        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And the "success" relationship of the PutS3Object processor is connected to the PutFile

    And a s3 server is set up in correspondence with the PutS3Object
    And the http proxy server is set up
    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 150 seconds
    And the object on the s3 server is "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And the object content type on the s3 server is "application/octet-stream" and the object metadata matches use metadata
    And no errors were generated on the http-proxy regarding "http://s3-server:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can remove s3 bucket objects
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And a DeleteS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here:
      | source name    | relationship name | destination name |
      | GetFile        | success           | PutS3Object      |
      | PutS3Object    | success           | DeleteS3Object   |
      | DeleteS3Object | success           | PutFile          |

    And a s3 server is set up in correspondence with the PutS3Object

    When both instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 120 seconds
    And the object bucket on the s3 server is empty

  Scenario: Deletion of a non-existent s3 object succeeds
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a DeleteS3Object processor set up to communicate with an s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the DeleteS3Object
    And the "success" relationship of the DeleteS3Object processor is connected to the PutFile

    And a s3 server is set up in correspondence with the DeleteS3Object

    When both instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 120 seconds
    And the object bucket on the s3 server is empty

  Scenario: Deletion of a s3 object through a proxy-server succeeds
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And a DeleteS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And these processor properties are set to match the http proxy:
      | processor name  | property name  | property value |
      | DeleteS3Object     | Proxy Host     | http-proxy     |
      | DeleteS3Object     | Proxy Port     | 3128           |
      | DeleteS3Object     | Proxy Username | admin          |
      | DeleteS3Object     | Proxy Password | test101        |
    And the processors are connected up as described here:
      | source name    | relationship name | destination name |
      | GetFile        | success           | PutS3Object      |
      | PutS3Object    | success           | DeleteS3Object   |
      | DeleteS3Object | success           | PutFile          |

    And a s3 server is set up in correspondence with the PutS3Object
    And the http proxy server is set up

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 150 seconds
    And the object bucket on the s3 server is empty
    And no errors were generated on the http-proxy regarding "http://s3-server:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can download s3 bucket objects directly
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a GenerateFlowFile processor with the "File Size" property set to "1 kB" in a "secondary" flow
    And a FetchS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here:
      | source name      | relationship name | destination name |
      | GenerateFlowFile | success           | FetchS3Object    |
      | FetchS3Object    | success           | PutFile          |

    And a s3 server is set up in correspondence with the PutS3Object

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 120 seconds

  Scenario: A MiNiFi instance can download s3 bucket objects via a http-proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a GenerateFlowFile processor with the "File Size" property set to "1 kB" in a "secondary" flow
    And a FetchS3Object processor set up to communicate with the same s3 server
    And these processor properties are set to match the http proxy:
      | processor name | property name  | property value |
      | FetchS3Object  | Proxy Host     | http-proxy     |
      | FetchS3Object  | Proxy Port     | 3128           |
      | FetchS3Object  | Proxy Username | admin          |
      | FetchS3Object  | Proxy Password | test101        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here:
      | source name      | relationship name | destination name |
      | GenerateFlowFile | success           | FetchS3Object    |
      | FetchS3Object    | success           | PutFile          |

    And a s3 server is set up in correspondence with the PutS3Object
    And a http proxy server is set up accordingly

    When all instances start up

    Then a flowfile with the content "test" is placed in the monitored directory in less than 150 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can list an S3 bucket directly
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Batch Size" property of the GetFile processor is set to "1"
    And the scheduling period of the GetFile processor is set to "5 sec"
    And a file with filename "test_file_1.log" and content "test_data1" is present in "/tmp/input"
    And a file with filename "test_file_2.log" and content "test_data2" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "Object Key" property of the PutS3Object processor is set to "${filename}"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a ListS3 processor in the "secondary" flow
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListS3 processor is connected to the PutFile

    And a s3 server is set up in correspondence with the PutS3Object

    When all instances start up

    Then 2 flowfiles are placed in the monitored directory in 120 seconds

  Scenario: A MiNiFi instance can list an S3 bucket objects via a http-proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a file with the content "test" is present in "/tmp/input"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a ListS3 processor in the "secondary" flow
    And these processor properties are set to match the http proxy:
      | processor name | property name  | property value |
      | ListS3         | Proxy Host     | http-proxy     |
      | ListS3         | Proxy Port     | 3128           |
      | ListS3         | Proxy Username | admin          |
      | ListS3         | Proxy Password | test101        |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListS3 processor is connected to the PutFile

    And a s3 server is set up in correspondence with the PutS3Object
    And a http proxy server is set up accordingly

    When all instances start up

    Then 1 flowfile is placed in the monitored directory in 120 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server:9090/test_bucket"

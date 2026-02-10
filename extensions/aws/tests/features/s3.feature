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

@ENABLE_AWS
Feature: Sending data from MiNiFi-C++ to an AWS server
  In order to transfer data to interact with AWS servers
  As a user of MiNiFi
  I need to have PutS3Object and DeleteS3Object processors

  Scenario: A MiNiFi instance transfers encoded data to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And a PutS3Object processor set up to communicate with an s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And the "success" relationship of the PutS3Object processor is connected to the PutFile
    And the "failure" relationship of the PutS3Object processor is connected to the PutS3Object
    And PutFile's success relationship is auto-terminated

    And an s3 server is set up

    When the MiNiFi instance starts up

    Then a single file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object on the s3 server is "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And the object content type on the s3 server is "application/octet-stream" and the object metadata matches use metadata
    And the Minifi logs contain the following message: "in a single upload" in less than 10 seconds

  Scenario: A MiNiFi instance transfers encoded data through a http proxy to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And a PutS3Object processor set up to communicate with an s3 server
    And these processor properties are set
      | processor name  | property name  | property value                  |
      | PutS3Object     | Proxy Host     | http-proxy-${scenario_id}        |
      | PutS3Object     | Proxy Port     | 3128                            |
      | PutS3Object     | Proxy Username | admin                           |
      | PutS3Object     | Proxy Password | test101                         |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And the "success" relationship of the PutS3Object processor is connected to the PutFile
    And the "failure" relationship of the PutS3Object processor is connected to the PutS3Object
    And PutFile's success relationship is auto-terminated

    And an s3 server is set up
    And the http proxy server is set up
    When all instances start up

    Then a single file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object on the s3 server is "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And the object content type on the s3 server is "application/octet-stream" and the object metadata matches use metadata
    And no errors were generated on the http-proxy regarding "http://s3-server-s3-1:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can remove s3 bucket objects
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And a PutS3Object processor set up to communicate with an s3 server
    And a DeleteS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here
      | source name    | relationship name | destination name |
      | GetFile        | success           | PutS3Object      |
      | PutS3Object    | success           | DeleteS3Object   |
      | PutS3Object    | failure           | PutS3Object      |
      | DeleteS3Object | success           | PutFile          |
      | PutFile        | success           | auto-terminated  |

    And an s3 server is set up

    When the MiNiFi instance starts up

    Then a single file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object bucket on the s3 server is empty in less than 10 seconds

  Scenario: Deletion of a non-existent s3 object succeeds
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a DeleteS3Object processor set up to communicate with an s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the GetFile processor is connected to the DeleteS3Object
    And the "success" relationship of the DeleteS3Object processor is connected to the PutFile

    And an s3 server is set up

    When the MiNiFi instance starts up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object bucket on the s3 server is empty in less than 10 seconds

  Scenario: Deletion of a s3 object through a proxy-server succeeds
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{"
    And a PutS3Object processor set up to communicate with an s3 server
    And a DeleteS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And these processor properties are set
      | processor name  | property name  | property value                  |
      | DeleteS3Object  | Proxy Host     | http-proxy-${scenario_id}        |
      | DeleteS3Object  | Proxy Port     | 3128                            |
      | DeleteS3Object  | Proxy Username | admin                           |
      | DeleteS3Object  | Proxy Password | test101                         |
    And the processors are connected up as described here
      | source name    | relationship name | destination name |
      | GetFile        | success           | PutS3Object      |
      | PutS3Object    | failure           | PutS3Object      |
      | PutS3Object    | success           | DeleteS3Object   |
      | DeleteS3Object | success           | PutFile          |
      | PutFile        | success           | auto-terminated  |

    And an s3 server is set up
    And the http proxy server is set up

    When all instances start up

    Then a single file with the content "LH_O#L|FD<FASD{FO#@$#$%^ \"#\"$L%:\"@#$L\":test_data#$#%#$%?{\"F{" is placed in the "/tmp/output" directory in less than 20 seconds
    And the object bucket on the s3 server is empty in less than 10 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server-s3-4:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can download s3 bucket objects directly
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutS3Object processor set up to communicate with an s3 server
    And a GenerateFlowFile processor with the "File Size" property set to "1 kB"
    And a FetchS3Object processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here
      | source name      | relationship name | destination name |
      | GenerateFlowFile | success           | FetchS3Object    |
      | FetchS3Object    | success           | PutFile          |
      | PutFile          | success           | auto-terminated  |
      | PutFile          | failure           | auto-terminated  |
      | GetFile          | success           | PutS3Object      |
      | PutS3Object      | success           | auto-terminated  |
      | PutS3Object      | failure           | PutS3Object      |

    And an s3 server is set up

    When the MiNiFi instance starts up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 20 seconds

  Scenario: A MiNiFi instance can download s3 bucket objects via a http-proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a GenerateFlowFile processor with the "File Size" property set to "1 kB"
    And a FetchS3Object processor set up to communicate with the same s3 server
    And these processor properties are set
      | processor name | property name  | property value                  |
      | FetchS3Object  | Proxy Host     | http-proxy-${scenario_id}       |
      | FetchS3Object  | Proxy Port     | 3128                            |
      | FetchS3Object  | Proxy Username | admin                           |
      | FetchS3Object  | Proxy Password | test101                         |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the processors are connected up as described here
      | source name      | relationship name | destination name |
      | GenerateFlowFile | success           | FetchS3Object    |
      | FetchS3Object    | success           | PutFile          |

    And an s3 server is set up
    And the http proxy server is set up

    When all instances start up

    Then a single file with the content "test" is placed in the "/tmp/output" directory in less than 20 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server-s3-6:9090/test_bucket/test_object_key"

  Scenario: A MiNiFi instance can list an S3 bucket directly
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And the "Batch Size" property of the GetFile processor is set to "1"
    And the scheduling period of the GetFile processor is set to "5 sec"
    And a directory at "/tmp/input" has a file ("test_file_1.log") with the content "test_data1"
    And a directory at "/tmp/input" has a file ("test_file_2.log") with the content "test_data2"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "Object Key" property of the PutS3Object processor is set to "${filename}"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a ListS3 processor set up to communicate with the same s3 server
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListS3 processor is connected to the PutFile

    And an s3 server is set up

    When the MiNiFi instance starts up

    Then 2 files are placed in the "/tmp/output" directory in less than 20 seconds

  Scenario: A MiNiFi instance can list an S3 bucket objects via a http-proxy
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And a directory at "/tmp/input" has a file with the content "test"
    And a PutS3Object processor set up to communicate with an s3 server
    And the "success" relationship of the GetFile processor is connected to the PutS3Object

    Given a ListS3 processor set up to communicate with the same s3 server
    And these processor properties are set
      | processor name | property name  | property value                  |
      | ListS3         | Proxy Host     | http-proxy-${scenario_id}       |
      | ListS3         | Proxy Port     | 3128                            |
      | ListS3         | Proxy Username | admin                           |
      | ListS3         | Proxy Password | test101                         |
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the ListS3 processor is connected to the PutFile

    And an s3 server is set up
    And the http proxy server is set up

    When all instances start up

    Then 1 file is placed in the "/tmp/output" directory in less than 20 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server-s3-8:9090/test_bucket"

  Scenario: A MiNiFi instance transfers data in multiple parts to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And there is a 6MB file at the "/tmp/input" directory and we keep track of the hash of that
    And a PutS3Object processor set up to communicate with an s3 server
    And the "Multipart Threshold" property of the PutS3Object processor is set to "5 MB"
    And the "Multipart Part Size" property of the PutS3Object processor is set to "5 MB"
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the PutS3Object processor is connected to the PutFile
    And an s3 server is set up

    When the MiNiFi instance starts up

    Then 1 file is placed in the "/tmp/output" directory in less than 20 seconds
    And the object on the s3 server is present and matches the original hash
    And the Minifi logs contain the following message: "passes the multipart threshold, uploading it in multiple parts" in less than 10 seconds

  Scenario: A MiNiFi instance can use multipart upload through http proxy to s3
    Given a GetFile processor with the "Input Directory" property set to "/tmp/input"
    And there is a 6MB file at the "/tmp/input" directory and we keep track of the hash of that
    And a PutS3Object processor set up to communicate with an s3 server
    And the "Multipart Threshold" property of the PutS3Object processor is set to "5 MB"
    And the "Multipart Part Size" property of the PutS3Object processor is set to "5 MB"
    And these processor properties are set
    | processor name  | property name  | property value           |
    | PutS3Object     | Proxy Host     | http-proxy-${scenario_id} |
    | PutS3Object     | Proxy Port     | 3128                     |
    | PutS3Object     | Proxy Username | admin                    |
    | PutS3Object     | Proxy Password | test101                  |
    And the "success" relationship of the GetFile processor is connected to the PutS3Object
    And a PutFile processor with the "Directory" property set to "/tmp/output"
    And the "success" relationship of the PutS3Object processor is connected to the PutFile

    And an s3 server is set up
    And the http proxy server is set up
    When all instances start up

    Then 1 file is placed in the "/tmp/output" directory in less than 20 seconds
    And the object on the s3 server is present and matches the original hash
    And the Minifi logs contain the following message: "passes the multipart threshold, uploading it in multiple parts" in less than 10 seconds
    And no errors were generated on the http-proxy regarding "http://s3-server-s3-10:9090/test_bucket/test_object_key"

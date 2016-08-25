<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# Apache NiFi -  MiNiFi - C++

MiNiFi is a child project effort of Apache NiFi.  This repository is for a native implementation in C++.

## Table of Contents

- [License](#license)

## License

Except as otherwise noted this software is licensed under the
[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
## Dependencies
   * gcc - 4.8.4
   * g++ - 4.8.4
   * [libxml2](http://xmlsoft.org/) - tested with 2.9.1
     MAC: brew install libxml2
   * [libuuid] https://sourceforge.net/projects/libuuid/
     MAC: After download the above source, configure/make/make install

## Build instructions

Build application, it will build minifi exe under build and copy over to target directory
 
   $ make

Clean 
   
   $ make clean

## Running 

Running application

   $ ./target/minifi

The Native MiNiFi example flow.xml is in target/conf
It show cases a Native MiNiFi client which can generate flowfile, log flowfile and push it to the NiFi server.
Also it can pull flowfile from NiFi server and log the flowfile.
The NiFi server config is target/conf/flow_Site2SiteServer.xml

For trial command control protocol between Native MiNiFi and NiFi Server, please see the example NiFi Server implementation in test/Server.cpp
The command control protocol is not finalized yet. 

Caveat:
1) 
Add new propery HostName and Port into RemoteProcessGroup InputOutput port for remote Site2Site hostname and port
<remoteProcessGroup>
      <id>8f3b248f-d493-4269-b317-36f85719f480</id>
      <name>NiFi Flow</name>
      <url>http://localhost:8081/nifi</url>
      <timeout>30 sec</timeout>
      <yieldPeriod>1 sec</yieldPeriod>
      <transmitting>true</transmitting>
      <inputPort>
        <id>471deef6-2a6e-4a7d-912a-81cc17e3a204</id>
        <name> From Node A</name>
        <position x="0.0" y="0.0"/>
        <comments/>
        <scheduledState>RUNNING</scheduledState>
        <maxConcurrentTasks>1</maxConcurrentTasks>
        <useCompression>false</useCompression>
        <property>
            <name>Host Name</name>
                <value>localhost</value>
        </property>
        <property>
            <name>Port</name>
            <value>10001</value>
        </property>
      </inputPort>
2)
Add new proerties into minifi.properties for command control 
# MiNiFi Server for Command Control
nifi.server.name=localhost
nifi.server.port=9000
nifi.server.report.interval=1000 ms

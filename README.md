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
   * [LevelDB](https://github.com/google/leveldb) - tested with v1.18
     MAC: brew install leveldb
   * gcc - 4.8.4
   * g++ - 4.8.4
   * [libxml2](http://xmlsoft.org/) - tested with 2.9.1
     MAC: brew install libxml2
   * [libuuid] https://sourceforge.net/projects/libuuid/
     MAC: After download the above source, configure/make/make install

## Build instructions

Build application
 
   $ make clean
   $ make

Build tests
   
   $ make tests

Clean 
   
   $ make clean


## Running 

Running application
The nifi flow.xml and nifi.properties are in target/conf
   $ ./target/minifi

Runnning tests 

   $ ./build/FlowFileRecordTest 

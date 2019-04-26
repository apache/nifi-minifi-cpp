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

# Apache NiFi - MiNiFi - JNI Readme


This readme defines the configuration parameters to use JNI functionality within MiNiFi C++

## Table of Contents

- [Description](#description)
- [Configuration](#configuration)

## Description

JNI provides the ability to access NiFi processors within MiNiFi C++. By exploding NARs, and coupling the framework
JNI jar that exist within the JNI extension, we can replicate the behavior of NiFi processors.

The subdirectory nifi-framework-jni contains the corresponding JNI library that is needed. Place that into the API directory if you are not running
make package. 

The `make package` process will build all necessary JARS and NARS if maven is available on the classpath. 

## Configuration

To enable JNI capabilities, the following options need to be provided in minifi.properties

    in minifi.properties
	#directory where base API exists.
	nifi.framework.dir=./minifi-jni/api
	
	# directory where NARs are located
	nifi.nar.directory=<nar directory>
	# directory where nars will be deployed
	nifi.nar.deploy.directory=<deploy directory>
	
Optionally, you can specify JVM options in a comma separated list
	
	# must be comma separated 
	nifi.jvm.options=-Xmx1G

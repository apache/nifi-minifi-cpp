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

# Apache NiFi - MiNiFi - Python Processors Readme


This readme defines the configuration parameters to use ExecutePythonProcessor to run native python processors.

## Table of Contents

- [Description](#description)
- [Configuration](#configuration)

## Description

Python native processors can be updated at any time by simply adding a new processor to the directory defined in
the configuration options. The processor name, when provided to MiNiFi C++ and any C2 manifest will be that
of the name of the python script. For example, "AttributePrinter.py" will be named and referenced in the flow
as "org.apache.nifi.minifi.processors.AttributePrinter" 

Methods that are enabled within the processor are  describe, onSchedule, onInitialize, and onTrigger. 

Describe is passed the a processor and is a required function. You must set the description like so:

```python
	def describe(processor):
	  processor.setDescription("Adds an attribute to your flow files")
```
   
   
   void addProperty(const std::string &name, const std::string &description, const std::string &defaultvalue, bool required, bool el) {
   
onInitialize is also passed the processor reference and can be where you set properties. The first argument is the property display name,
followed by the description, and default value. The last two arguments are booleans describing if the property is required or requires EL.

```python
	def onIitialize(processor):
	  processor.setSupportsDynamicProperties()
	  processor.addProperty("property name","description","default value", True, False)
```

The onSchedule function is passed the context and session factory. This should be where your processor loads and reads properties via
the getProperty function. onTrigger is executed and passed the processor context and session. You may keep state within your processor.

## Configuration

To enable python Processor capabilities, the following options need to be provided in minifi.properties

    in minifi.properties
	#directory where processors exist
	nifi.python.processor.dir=XXXX
	
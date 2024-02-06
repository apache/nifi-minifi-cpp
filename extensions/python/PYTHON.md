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

- [Requirements](#requirements)
- [Description](#description)
- [Configuration](#configuration)
- [Processors](#processors)
- [Using NiFi Python Processors](#using-nifi-python-processors)


## Requirements
This extension targets the 3.6 stable python API, this means it will work with any(≥3.6) python library.

### CentOS/RHEL system python
```
yum install python3-libs
```

### Debian/Ubuntu system python
```
apt install libpython3-dev
```

Debian/Ubuntu doesn't provide the generic libpython3.so, but the extension works with the specific libraries as well.
To use the extension on a system where the generic libpython3.so is not available, we must patch the library to use the specific library.

e.g. This will change the dependency from the generic libpython3.so to the specific libpython3.9.so
```shell
patchelf extensions/libminifi-python-script-extension.so --replace-needed libpython3.so libpython3.9.so
```

### Windows system python
When installing python on Windows, make sure to select the option to install python for all users. This prevents issues when running MiNiFi as a Windows service, as it makes sure
that the python libraries are available not just for the currently logged on user but for the user running the service too.

If the python libraries are not available for the user running the service, MiNiFi starts with an error message similar to this:
```
Failed to load extension 'minifi-python-script-extension' at 'C:\Program Files\ApacheNiFiMiNiFi\nifi-minifi-cpp\bin\..\extensions\minifi-python-script-extension.dll': The specified module could not be found.
```

### Anaconda
Just make sure minifi finds the anaconda libraries. e.g.:
```shell
export LD_LIBRARY_PATH="${CONDA_PREFIX}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
```

### PyEnv
Just make sure minifi finds the pyenv libraries. e.g.:

```shell
export LD_LIBRARY_PATH="${PYENV_ROOT}/versions/${PY_VERSION}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
```

## Description

Python native processors can be updated at any time by simply adding a new processor to the directory defined in
the configuration options. The processor name, when provided to MiNiFi C++ and any C2 manifest will be that
of the name of the python script. For example, "AttributePrinter.py" will be named and referenced in the flow
as "org.apache.nifi.minifi.processors.AttributePrinter"

Methods that are enabled within the processor are  describe, onSchedule, onInitialize, and onTrigger.

Describe is passed the processor and is a required function. You must set the description like so:

```python
def describe(processor):
  processor.setDescription("Adds an attribute to your flow files")
```

onInitialize is also passed the processor reference and can be where you set properties. The first argument is the property display name,
followed by the description, and default value. The next three arguments are booleans describing if the property is required, support expression language, and if it is a sensitive property.
The last argument is the property type code. The property type code is an integer that represents the type of the property. The supported property type codes and their corresponding types:
```
INTEGER = 0
LONG = 1
BOOLEAN = 2
DATA_SIZE = 3
TIME_PERIOD = 4
NON_BLANK = 5
PORT = 6
```

```python
def onInitialize(processor):
  processor.setSupportsDynamicProperties()
  processor.addProperty("property name","description","default value", True /*required*/, False /*expression language supported*/, False /*sensitive*/, 1 /*property type code*/)
```

The onSchedule function is passed the context and session factory. This should be where your processor loads and reads properties via
the getProperty function. onTrigger is executed and passed the processor context and session. You may keep state within your processor.

Much like C++ processors, callbacks may be defined for reading/writing streams of data through the session. Those callback classes will
have a process function that accepts the input stream. You may use codecs getReader to read that data as in the example, below, from
VaderSentiment

```python
class VaderSentiment(object):
  def __init__(self):
    self.content = None

  def process(self, input_stream):
    self.content = codecs.getreader('utf-8')(input_stream).read()
    return len(self.content)
```

## Configuration

To enable python Processor capabilities, the following options need to be provided in minifi.properties. The directory specified
can contain processors. Note that the processor name will be the reference in your flow. Directories are treated like package names.
Therefore if the nifi.python.processor.dir is /tmp/ and you have a subdirectory named packagedir with the file name file.py, it will
produce a processor with the name org.apache.nifi.minifi.processors.packagedir.file. Note that each subdirectory will append a package
to the reference class name.

    in minifi.properties
	#directory where processors exist
	nifi.python.processor.dir=XXXX


## Processors
The python directory (extensions/pythonprocessors) contains implementations that will be available for flows if the required dependencies
exist.

### Sentiment Analysis

The SentimentAnalysis processor will perform a Vader Sentiment Analysis. This requires that you install nltk and VaderSentiment
		pip install nltk
		pip install VaderSentiment

## Using NiFi Python Processors

MiNiFi C++ supports the use of NiFi Python processors, that are inherited from the FlowFileTransform base class. To use these processors, you must copy the Python processor module under the nifi_python_processors directory located under the python directory (by default it can be found at ${minifi_root}/minifi-python/nifi_python_processors). To see how to to write NiFi Python processors, please refer to the Python Developer Guide under the [Apache NiFi documentation](https://nifi.apache.org/documentation/v2/).

In the flow configuration these Python processors can be referenced by their fully qualified class name, which looks like this: org.apache.nifi.minifi.processors.nifi_python_processors.<package_name>.<processor_name>. For example, the fully qualified class name of the PromptChatGPT processor implemented in the file nifi_python_processors/PromptChatGPT.py is org.apache.nifi.minifi.processors.nifi_python_processors.PromptChatGPT. If a processor is copied under a subdirectory, because it is part of a python submodule, the submodule name will be appended to the fully qualified class name. For example, if the QueryPinecone processor is implemented in the QueryPinecone.py file that is copied to nifi_python_processors/vectorstores/QueryPinecone.py, the fully qualified class name will be org.apache.nifi.minifi.processors.nifi_python_processors.vectorstores.QueryPinecone in the configuration file.

**NOTE:** The name of the NiFi Python processor file should match the class name in the file, otherwise the processor will not be found.

Due to some differences between the NiFi and MiNiFi C++ processors and implementation, there are some limitations using the NiFi Python processors:
- Record based processors are not yet supported in MiNiFi C++, so the NiFi Python processors inherited from RecordTransform are not supported.
- Virtualenv support is not yet available in MiNiFi C++, so all required packaged must be installed on the system.
- Controller properties are not supported at the moment.
- There are some validators in NiFi that are not present in MiNiFi C++, so some property validations will be missing using the NiFi Python processors.
- Allowable values specified in NiFi Python processors are ignored in MiNiFi C++ (due to MiNiFi C++ requiring them to be specified at compile time), so the property values are not pre-verified.
- MiNiFi C++ does not support custom relationship names in Python processors, the only available relationships are "success", "failure" and "original".
- MiNiFi C++ only supports expression language with flow file attributes, so only FLOWFILE_ATTRIBUTES expression language scope is supported, otherwise the expression language will not be evaluated.
- MiNiFi C++ does not support property dependencies, so the property dependencies will be ignored. If a property depends on another property, the property will not be required.
- MiNiFi C++ does not support the use of self.jvm member in Python processors that provides JVM bindings in NiFi, it is set to None in MiNiFi C++.

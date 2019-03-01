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

# Apache NiFi - MiNiFi - C++ Python Access.


This readme provides a how-to guide on using the Python bindings for MiNiFi C++. 

## Table of Contents

- [Description](#description)
- [Enabling](#enabling)
- [Example](#example)
- [Limitations](#limitations)

## Description

Apache NiFi MiNiFi C++ can communicate using python bindings. These bindings connect
to the existing C API. In doing so, they can utilize the building blocks within the CAPI.

The design is predicated upon a MiNiFi instance. There is a getFile example that shows
the usage of this API. A processor can be created and then a flowfile will be output if one
is routed to success. Custom routes can be defined in later implementations of the Python API.

An RPG is currently required to define a MiNiFi instance. As per the example, a flow file may 
be transmitted via HTTP site to site. Presently, raw socket site to site is supported via
the CAPI but not the Python library.

To run the getfile example you will need to provide the fullpath to the python-lib shared object
created at build time.

## Enabling
	At build time you must specify -DENABLE_PYTHON=ON to enable python bindings to be built.
	
	Further, you may have to install CFFI for python bindings.
	
	This can typically be performed through pip, with `pip install cffi`
	
## Example
   The python directory contains an example where we use a MiNiFi C++ processor along with a
   a python processor. The implementation of the python processor requires that a call back
   method be defined for ontrigger.
   
## Limitations
   Python bindings currently don't build on WIN32.

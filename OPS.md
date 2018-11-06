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

# Apache NiFi - MiNiFi - Operations Readme.


This readme defines operational commands for managing instances.

## Table of Contents

- [Description](#description)
- [Managing](#managing-minifi)
  - [Commands](#commands)

## Description

Apache NiFi MiNiFi C++ can be managed through our [C2 protocol](https://cwiki.apache.org/confluence/display/MINIFI/C2+Design+Proposal) 
or through a local interface called the MiNiFi Controller. This feature is disabled by default, and requires that C2 be enabled
and configured with an agent class before using the MiNiFi controller features outlined here.

## Managing MiNiFi

The MiNiFi controller is an executable in the bin directory that can be used to control the MiNiFi C++ agent while it runs -- utilizing the [Command and Control Protocol](https://cwiki.apache.org/confluence/display/MINIFI/C2+Design+Proposal). Currently the controller will let you stop subcomponents within a running instance, clear queues, get the status of queues, and update the flow for a warm re-deploy.

The minificontroller can track a single MiNiFi C++ agent through the use of three options. Port is required.
The hostname is not and will default to localhost. Additionally, controller.socket.local.any.interface allows
you to bind to any address when using localhost. Otherwise, we will bind only to the loopback adapter so only
minificontroller on the local host can control the agent:

	$ controller.socket.host=localhost
	$ controller.socket.port=9998
	$ controller.socket.local.any.interface=true/false (default: false)

These are defined by default to the above values. If the port option is left undefined, the MiNiFi controller
will be disabled in your deployment.

 The executable is stored in the bin directory and is titled minificontroller. Available commands are listed below.
 Note that with all commands an immediate response by the agent isn't guaranteed. In all cases the agent assumes the role of validating that a response was received, but execution of said command may take some time depending on a number of factors to include persistent storage type, size of queues, and speed of hardware. 
 
### Debug
  
  Agents have the ability to return a list of stacks of currently running threads. The Jstack command provides a list of call stacks
  for threads within the agent. This may allow users and maintainers to view stacks of running threads to diagnose issues. The name
  is an homage to the jstack command used by Java developers. The design is fundamentally the same as that of Java -- signal handlers
  notify signals to interrupt and provide traces. This feature is currently not built into Windows builds.
 
### Commands
 #### Specifying connecting information
 
   ./minificontroller --host "host name" --port "port"

        * By default these options use those defined in minifi.properties and are not required

 #### Start Command
 
   ./minificontroller --start "component name"
 
 #### Stack command
   ./minificontroller --jstack
    
 #### Stop command 
   ./minificontroller --stop "component name"
   	  
 #### List connections command
   ./minificontroller --list connections
      
 #### List components command
   ./minificontroller --list components
 
 #### Clear connection command
   ./minificontroller --clear "connection name"
      
 #### GetSize command
   ./minificontroller --getsize "connection name"

       * Returns the size of the connection. The current size along with the max will be reported
 
 #### Update flow
   ./minificontroller --updateflow "config yml"
    
       * Updates the flow file reference and performs a warm re-deploy.
 
 #### Get full connection command     
   ./minificontroller --getfull 
   
       * Provides a list of full connections, if any.

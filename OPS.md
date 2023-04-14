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

Apache NiFi MiNiFi C++ can be managed through our [C2 protocol](https://cwiki.apache.org/confluence/display/MINIFI/C2+Design)
or through a local interface called the MiNiFi Controller. This feature is disabled by default, and requires to be enabled
through the minifi.properties of the agent before using the MiNiFi controller features outlined here. MiNiFi controller is an example
implementation of our C2 protocol. This featureset is not intended to replace your Command and Control implementation. Instead
it is meant to provide testing and minimal operational capabilities by example.

## Managing MiNiFi

The MiNiFi controller is an executable in the bin directory that can be used to control the MiNiFi C++ agent while it runs -- utilizing the [Command and Control Protocol](https://cwiki.apache.org/confluence/display/MINIFI/C2+Design). Currently the controller will let you stop subcomponents within a running instance, clear queues, get the status of queues, and update the flow for a warm re-deploy. It also provides an option to retrieve the agent manifest json.

This functionality has to enabled in the minifi.properties file with the controller.socket.enable property.
The minificontroller can track a single MiNiFi C++ agent through the use of three options. Port is required.
The hostname is not and will default to localhost. Additionally, controller.socket.local.any.interface allows
you to bind to any address when using localhost. Otherwise, we will bind only to the loopback adapter so only
minificontroller on the local host can control the agent:

    $ controller.socket.enable=true
    $ controller.socket.host=localhost
    $ controller.socket.port=9998
    $ controller.socket.local.any.interface=true/false (default: false)

These are defined by default to the above values. If the port option is left undefined, the MiNiFi controller
will be disabled in your deployment.

The executable is stored in the bin directory and is titled minificontroller. Available commands are listed below.

### SSL

To use secure connection for the C2 commands, both the MiNiFi C++ agent and the controller have to be started with either of the following configurations.

To retrieve the SSL configuration properties from the flow config, set the following property to the name of the SSL context service holding the information:

    $ controller.ssl.context.service=SSLContextService

Otherwise if you prefer to retrieve the SSL configuration properties from the minifi.properties file set the nifi.remote.input.secure property value to true and configure the security properties in the minifi.properties file.

    $ nifi.remote.input.secure=true
    $ nifi.security.client.certificate=/path/to/cert/mycert.crt
    $ nifi.security.client.private.key=/path/to/private/key/myprivatekey.key
    $ nifi.security.client.pass.phrase=passphrase
    $ nifi.security.client.ca.certificate=/path/to/ca/cert/myrootca.pem

### Commands

#### Specifying connecting information
    ./minificontroller --host "host name" --port "port"

By default these options use those defined in minifi.properties and are not required

#### Start Command
    ./minificontroller --start "component name"

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

Returns the size of the connection. The current size along with the max will be reported

#### Update flow
    ./minificontroller --updateflow "config yml"

Updates the flow file reference and performs a warm re-deploy.

#### Get full connection command
    ./minificontroller --getfull

Provides a list of full connections, if any.

#### Get manifest command
    ./minificontroller --manifest

Writes the agent manifest json to standard output

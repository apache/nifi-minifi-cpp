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

## Table of Contents
- [Site-to-Site Overview](#site-to-site-overview)
- [Site-to-Site Configuration](#site-to-site-configuration)
  - [Site-to-Site Configuration on NiFi side](#site-to-site-configuration-on-nifi-side)
  - [Site-to-Site Configuration on MiNiFi C++ side](#site-to-site-configuration-on-minifi-c-side)
- [Additional examples](#additional-examples)

## Site-to-Site Overview

Site-to-Site protocol allows data to be transferred between MiNiFi C++ and NiFi instances. MiNiFi C++ can send or receive data from NiFi using remote process groups. This is useful for scenarios where you want to send data from MiNiFi C++ to NiFi or vice versa. Site-to-Site protocol support raw TCP and HTTP protocols.

At the moment site-to-site protocol is only supported between MiNiFi C++ and NiFi instances, it cannot be used to transfer data between multiple MiNiFi C++ instances. It is recommended to use processors like InvokeHTTP and ListenHTTP to transfer data between MiNiFi C++ instances.

## Site-to-Site Configuration

### Site-to-Site Configuration on NiFi side

On NiFi side, site-to-site protocol is configured by creating input and output ports. The input port is used to receive data from MiNiFi C++ and the output port is used to send data to MiNiFi C++. The input and output ports can be created in the NiFi UI by dragging and dropping the input and output port icons onto the canvas.

To use the input or output port of the NiFi flow in the MiNiFi C++ flow, the instance id of the port should be used. The instance id can be found in the NiFi UI by clicking on the input or output port and looking at the operation panel. It can be copied from that panel, or from the port "instanceIdentifier" field from configuration json file in the NiFi conf directory.

### Site-to-Site Configuration on MiNiFi C++ side

Site-to-Site protocol is configured on the MiNiFi C++ side by using remote process groups in the configuration. The remote process group represents the NiFi endpoint and uses the instance ids of the ports created on the NiFi side. The remote process group can be configured to use either raw TCP or HTTP protocol.

Here is a yaml example of how to configure site-to-site protocol in MiNiFi C++ where the MiNiFi C++ instance is sending data to NiFi using raw socket protocol:

```yaml
MiNiFi Config Version: 3
Flow Controller:
  name: Simple GenerateFlowFile to RPG
Processors:
  - id: b0c04f28-0158-1000-0000-000000000000
    name: GenerateFlowFile
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 5 sec
    auto-terminated relationships list: []
    Properties:
      Data Format: Text
      Unique FlowFiles: false
      Custom Text: Custom text
Connections:
  - id: b0c0c3cc-0158-1000-0000-000000000000
    name: GenerateFlowFile/succes/nifi-inputport
    source id: b0c04f28-0158-1000-0000-000000000000
    destination id: de7cc09a-0196-1000-2c63-ee6b4319ffb6
    source relationship name: success
Remote Process Groups:
  - id: b0c09ff0-0158-1000-0000-000000000000
    name: "RPG"
    url: http://localhost:8080/nifi
    timeout: 20 sec
    yield period: 10 sec
    transport protocol: RAW
    Input Ports:
      - id: de7cc09a-0196-1000-2c63-ee6b4319ffb6  # this is the instance id of the input port created in NiFi
        name: nifi-inputport
        max concurrent tasks: 1
        use compression: true
        batch size:
          size: 10 MB
          count: 10
          duration: 30 sec
    Output Ports: []
```

Here is another example in yaml format how to configure site-to-site protocol in MiNiFi C++ where the MiNiFi C++ instance is receiving data from NiFi using the HTTP protocol:

```yaml
MiNiFi Config Version: 3
Flow Controller:
  name: MiNiFi Flow
Processors:
- Properties:
    Directory: /tmp/output
  auto-terminated relationships list:
  - success
  - failure
  class: org.apache.nifi.processors.standard.PutFile
  id: 6d6917dd-02ca-4add-b1e8-91468873009e
  max concurrent tasks: 1
  name: PutFile
  penalization period: 30 sec
  run duration nanos: 0
  scheduling period: 1 sec
  scheduling strategy: EVENT_DRIVEN
  yield period: 1 sec
Connections:
- destination id: 6d6917dd-02ca-4add-b1e8-91468873009e
  name: 64b65a70-4560-4717-89bb-d8335db99f27
  source id: 22d38f35-4d25-4e68-878c-f46f46d5781c
  source relationship name: undefined
Remote Processing Groups:
- Output Ports:
  - Properties: {}
    id: 22d38f35-4d25-4e68-878c-f46f46d5781c
    max concurrent tasks: 1
    name: from_nifi
    use compression: true
    batch size:
      size: 10 MB
      count: 10
      duration: 30 sec
  id: 20ed42b0-d41e-4add-9e6d-8777223370b8
  name: RemoteProcessGroup
  timeout: 30 sec
  url: http://localhost:8080/nifi
  yield period: 3 sec
```

Notes on the configuration:

- In the MiNiFi C++ configuration, in yaml configuration the remote input and output ports' `id` field, and in json configuration the ports' `identifier`, `instanceIdentifier`, and `targetId` fields should be set to the instance id of the input and output ports created in NiFi (`de7cc09a-0196-1000-2c63-ee6b4319ffb6` in the examples).
- Connections from the remote output port to the processor should use the `undefined` relationship
- the `url` field (`targetUri` or `targetUris` in JSON) field in the remote process group should be set to the NiFi instance's URL, this can also use comma separated list of URLs if the remote process group is configured to use multiple NiFi nodes

## Additional examples

You can check out some additional examples of using site-to-site protocol in this [bidirectional site-to-site example](examples/BidirectionalSiteToSite/README.md).

You can also check out a site-to-site example in [NiFi json](examples/site_to_site_config.nifi.schema.json), [MiNiFi C++ json](examples/site_to_site_config.json) and [yaml](examples/site_to_site_config.yml) formats.

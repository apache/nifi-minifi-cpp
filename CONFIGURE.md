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

# Apache NiFi -  Configuring MiNiFi - C++ 

## Configuring
The 'conf' directory in the root contains a template config.yml document.  

This is partly compatible with the format used with the Java MiNiFi application. MiNiFi C++ is currently compatible with version 1 of the MiNiFi YAML schema.
Additional information on the YAML format for the config.yml and schema versioning can be found in the [MiNiFi System Administrator Guide](https://nifi.apache.org/minifi/system-admin-guide.html).

MiNiFi Toolkit Converter (version 0.0.1 - schema version 1) is considered as deprecated from MiNiFi C++ 0.7.0. It was to aid in creating a flow configuration from a generated template exported from a NiFi instance. The MiNiFi Toolkit Converter tool can be downloaded from http://nifi.apache.org/minifi/download.html under the `MiNiFi Toolkit Binaries` section.  Information on its usage is available at https://nifi.apache.org/minifi/minifi-toolkit.html. Using the toolkit is no longer supported and maintained.

It's recommended to create your configuration in YAML format or configure the agent via Command and Control protocol (see below)


    Flow Controller:
        id: 471deef6-2a6e-4a7d-912a-81cc17e3a205
        name: MiNiFi Flow
        onschedule retry interval: 30000 ms

    Processors:
        - name: GetFile
          id: 471deef6-2a6e-4a7d-912a-81cc17e3a206
          class: org.apache.nifi.processors.standard.GetFile
          max concurrent tasks: 1
          scheduling strategy: TIMER_DRIVEN
          scheduling period: 1 sec
          penalization period: 30 sec
          yield period: 1 sec
          run duration nanos: 0
          auto-terminated relationships list:
          Properties:
              Input Directory: /tmp/getfile
              Keep Source File: true

    Connections:
        - name: TransferFilesToRPG
          id: 471deef6-2a6e-4a7d-912a-81cc17e3a207
          source name: GetFile
          source id: 471deef6-2a6e-4a7d-912a-81cc17e3a206
          source relationship name: success
          destination id: 471deef6-2a6e-4a7d-912a-81cc17e3a204
          max work queue size: 0
          max work queue data size: 1 MB
          flowfile expiration: 60 sec
          drop empty: false

    Remote Processing Groups:
        - name: NiFi Flow
          id: 471deef6-2a6e-4a7d-912a-81cc17e3a208
          url: http://localhost:8080/nifi
          timeout: 30 secs
          yield period: 10 sec
          Input Ports:
              - id: 471deef6-2a6e-4a7d-912a-81cc17e3a204
                name: From Node A
                max concurrent tasks: 1
                Properties:

### Scheduling strategies
Currently Apache NiFi MiNiFi C++ supports TIMER_DRIVEN, EVENT_DRIVEN, and CRON_DRIVEN. TIMER_DRIVEN uses periods to execute your processor(s) at given intervals.
The EVENT_DRIVEN strategy awaits for data be available or some other notification mechanism to trigger execution. CRON_DRIVEN executes at the desired intervals
based on the CRON periods. Apache NiFi MiNiFi C++ supports standard CRON expressions without intervals ( */5 * * * * ). 

### SiteToSite Security Configuration

    in minifi.properties

    enable tls
    nifi.remote.input.secure=true

    if you want to enable client certificate base authorization
    nifi.security.need.ClientAuth=true
    setup the client certificate and private key PEM files
    nifi.security.client.certificate=./conf/client.pem
    nifi.security.client.private.key=./conf/client.pem
    setup the client private key passphrase file
    nifi.security.client.pass.phrase=./conf/password
    setup the client CA certificate file
    nifi.security.client.ca.certificate=./conf/nifi-cert.pem

    if you do not want to enable client certificate base authorization
    nifi.security.need.ClientAuth=false
  
You have the option of specifying an SSL Context Service definition for the RPGs instead of the properties above. 
This will link to a corresponding SSL Context service defined in the flow. 
    
To do this specify the SSL Context Service Property in your RPGs and link it to 
a defined controller service. If you do not take this approach the options, above, will be used
for TCP and secure HTTPS communications.
    
    Remote Processing Groups:
    - name: NiFi Flow
      id: 2438e3c8-015a-1000-79ca-83af40ec1998
      url: http://127.0.0.1:8080/nifi
      timeout: 30 secs
      yield period: 5 sec
      Input Ports:
          - id: 2438e3c8-015a-1000-79ca-83af40ec1999
            name: fromnifi
            max concurrent tasks: 1
            Properties:
                SSL Context Service: SSLServiceName
      Output Ports:
          - id: ac82e521-015c-1000-2b21-41279516e19a
            name: tominifi
            max concurrent tasks: 2
            Properties:
		SSL Context Service: SSLServiceName
	Controller Services:
    - name: SSLServiceName
      id: 2438e3c8-015a-1000-79ca-83af40ec1974
      class: SSLContextService
      Properties:
          Client Certificate: <client cert path>
          Private Key: < private key path > 
          Passphrase: <passphrase path or passphrase>
          CA Certificate: <CA cert path>
    
### HTTP SiteToSite Configuration
To enable HTTPSiteToSite globally you must set the following flag to true.
	
    nifi.remote.input.http.enabled=true

To enable HTTPSiteToSite for a remote process group.
    Remote Processing Groups:
    - name: NiFi Flow
      transport protocol: HTTP
    
### HTTP SiteToSite Proxy Configuration
To enable HTTP Proxy for a remote process group.

    Remote Processing Groups:
    - name: NiFi Flow
      transport protocol: HTTP
      proxy host: localhost
      proxy port: 8888
      proxy user:
      proxy password:

### Command and Control Configuration
Please see the [C2 readme](C2.md) for more informatoin 
	
	
### Configuring Repository storage locations
Persistent repositories, such as the Flow File repository, use a configurable path to store data. 
The repository locations and their defaults are defined below. By default the MINIFI_HOME env
variable is used. If this is not specified we extrapolate the path and use the root installation
folder. You may specify your own path in place of these defaults. 
     
     in minifi.properties
     nifi.provenance.repository.directory.default=${MINIFI_HOME}/provenance_repository
     nifi.flowfile.repository.directory.default=${MINIFI_HOME}/flowfile_repository
	 nifi.database.content.repository.directory.default=${MINIFI_HOME}/content_repository

#### Shared database

It is also possible to use a single database to store multiple repositories with the `minifidb://` scheme.
This could help with migration and centralize agent state persistence. In the scheme the final path segment designates the
column family in the repository, while the preceding path indicates the directory the rocksdb database is
created into. E.g. in `minifidb:///home/user/minifi/agent_state/flowfile` a directory will be created at
`/home/user/minifi/agent_state` populated with rocksdb-specific content, and in that repository a logically
separate "subdatabase" is created under the name `"flowfile"`.

     in minifi.properties
     nifi.flowfile.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/flowfile
	 nifi.database.content.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/content
	 nifi.state.manangement.provider.local.path=minifidb://${MINIFI_HOME}/agent_state/processor_states

We should not simultaneously use the same directory with and without the `minifidb://` scheme.
Moreover the `"default"` name is restricted and should not be used.


     in minifi.properties
     nifi.flowfile.repository.directory.default=minifidb://${MINIFI_HOME}/agent_state/flowfile
	 nifi.database.content.repository.directory.default=${MINIFI_HOME}/agent_state
	 ^ error: using the same database directory without the "minifidb://" scheme
	 nifi.state.manangement.provider.local.path=minifidb://${MINIFI_HOME}/agent_state/default
	 ^ error: "default" is restricted

### Configuring Repository encryption

It is possible to provide rocksdb-backed repositories a key to request their
encryption.

    in conf/bootstrap.conf
    nifi.flowfile.repository.encryption.key=805D7B95EF44DC27C87FFBC4DFDE376DAE604D55DB2C5496DEEF5236362DE62E
    nifi.database.content.repository.encryption.key=
    # nifi.state.management.provider.local.encryption.key=

In the above configuration the first line will cause `FlowFileRepository` to use the specified `256` bit key.
The second line will trigger the generation of a random (`256` bit) key persisted back into `conf/bootstrap.conf`, which `DatabaseContentRepository` will then use for encryption.
(This way one can request encryption while not bothering with what key to use.)
Finally, as the last line is commented out, it will make the state manager use plaintext storage, and not trigger encryption.

#### Mixing encryption with shared backend

When multiple repositories use the same directory (as with `minifidb://` scheme) they should either be all plaintext or all encrypted with the same key.

### Configuring Volatile and NO-OP Repositories
Each of the repositories can be configured to be volatile ( state kept in memory and flushed
 upon restart ) or persistent. Currently, the flow file and provenance repositories can persist
 to RocksDB. The content repository will persist to the local file system if a volatile repo
 is not configured.

 To configure the repositories:

     in minifi.properties
     # For Volatile Repositories:
     nifi.flowfile.repository.class.name=VolatileFlowFileRepository
     nifi.provenance.repository.class.name=VolatileProvenanceRepository
     nifi.content.repository.class.name=VolatileContentRepository

     # configuration options
     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.flowfile.max.count=10000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.flowfile.max.bytes=1M

     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.provenance.max.count=10000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.provenance.max.bytes=1M

     # maximum number of entries to keep in memory
     nifi.volatile.repository.options.content.max.count=100000
     # maximum number of bytes to keep in memory, also limited by option above
     nifi.volatile.repository.options.content.max.bytes=1M
     # limits locking for the content repository
     nifi.volatile.repository.options.content.minimal.locking=true
     
     # For NO-OP Repositories:
	 nifi.flowfile.repository.class.name=NoOpRepository
     nifi.provenance.repository.class.name=NoOpRepository

 #### Caveats
 Systems that have limited memory must be cognizant of the options above. Limiting the max count for the number of entries limits memory consumption but also limits the number of events that can be stored. If you are limiting the amount of volatile content you are configuring, you may have excessive session rollback due to invalid stream errors that occur when a claim cannot be found.

 The content repository has a default option for "minimal.locking" set to true. This will attempt to use lock free structures. This may or may not be optimal as this requires additional additional searching of the underlying vector. This may be optimal for cases where max.count is not excessively high. In cases where object permanence is low within the repositories, minimal locking will result in better performance. If there are many processors and/or timing is such that the content repository fills up quickly, performance may be reduced. In all cases a locking cache is used to avoid the worst case complexity of O(n) for the content repository; however, this caching is more heavily used when "minimal.locking" is set to false.

### Provenance Reporter

    Add Provenance Reporting to config.yml
    Provenance Reporting:
      scheduling strategy: TIMER_DRIVEN
      scheduling period: 1 sec
      url: http://localhost:8080/nifi
      port uuid: 471deef6-2a6e-4a7d-912a-81cc17e3a204
      batch size: 100

### REST API access

    Configure REST API user name and password
    nifi.rest.api.user.name=admin
    nifi.rest.api.password=password

    if you want to enable client certificate
    nifi.https.need.ClientAuth=true
    nifi.https.client.certificate=./conf/client.pem
    nifi.https.client.private.key=./conf/client.key
    nifi.https.client.pass.phrase=./conf/password
    nifi.https.client.ca.certificate=./conf/nifi-cert.pem

### UID generation

MiNiFi needs to generate many unique identifiers in the course of operations.  There are a few different uid implementations available that can be configured in minifi-uid.properties.

Implementation for uid generation can be selected using the uid.implementation property values:
1. time - use uuid_generate_time (default option if the file or property value is missing or invalid)
2. random - use uuid_generate_random
3. uuid_default - use uuid_generate (will attempt to use uuid_generate_random and fall back to uuid_generate_time if no high quality randomness is available)
4. minifi_uid - use custom uid algorthim

If minifi_uuid is selected MiNiFi will use a custom uid algorthim consisting of first N bits device identifier, second M bits as bottom portion of a timestamp where N + M = 64, the last 64 bits is an atomic incrementor.

This is faster than the random uuid generator and encodes the device id and a timestamp into every value, making tracing of flowfiles, etc easier.

It does require more configuration.  uid.minifi.device.segment.bits is used to specify how many bits at the beginning to reserve for the device identifier.  It also puts a limit on how many distinct devices can be used.  With the default of 16 bits, there are a maximum of 65,535 unique device identifiers that can be used.  The 48 bit timestamp won't repeat for almost 9,000 years.  With 24 bits for the device identifier, there are a maximum of 16,777,215 unique device identifiers and the 40 bit timestamp won't repeat for about 35 years.

Additionally, a unique hexadecimal uid.minifi.device.segment should be assigned to each MiNiFi instance.

### Controller Services
 If you need to reference a controller service in your config.yml file, use the following template. In the example, below, ControllerServiceClass is the name of the class defining the controller Service. ControllerService1
 is linked to ControllerService2, and requires the latter to be started for ControllerService1 to start.

	Controller Services:
      - name: ControllerService1
 	    id: 2438e3c8-015a-1000-79ca-83af40ec1974
	  	class: ControllerServiceClass
	  	Properties:
	      Property one: value
	      Linked Services:
	        - value: ControllerService2
	  - name: ControllerService2
	    id: 2438e3c8-015a-1000-79ca-83af40ec1992
	  	class: ControllerServiceClass
	  	Properties:

### Linux Power Manager Controller Service
  The linux power manager controller service can be configured to monitor the battery level and status ( discharging or charging ) via the following configuration.
  Simply provide the capacity path and status path along with your threshold for the trigger and low battery alarm and you can monitor your battery and throttle
  the threadpools within MiNiFi C++. Note that the name is identified must be ThreadPoolManager.

    Controller Services:
    - name: ThreadPoolManager
      id: 2438e3c8-015a-1000-79ca-83af40ec1888
      class: LinuxPowerManagerService
      Properties:
          Battery Capacity Path: /path/to/battery/capacity
          Battery Status Path: /path/to/battery/status
          Trigger Threshold: 90
          Low Battery Threshold: 50
          Wait Period: 500 ms
          
### MQTT Controller service
The MQTTController Service can be configured for MQTT connectivity and provide that capability to your processors when MQTT is built.
    
    Controller Services:
    - name: mqttservice
      id: 294491a38-015a-1000-0000-000000000001
      class: MQTTContextService
      Properties:
          Broker URI: localhost:1883
    	    Client ID: client ID
          Quality of Service: 2
          
### Network Prioritizer Controller Service
  The network prioritizer controller service can be configured to manage prioritizing and binding to specific network interfaces. Linked Services, can be used
  as a prioritized list to create a disjunction among multiple networking prioritizers. This allows you to create classes with different configurations that
  create multiple prioritizations. Max Throughput is the maximum throughput in bytes per second. Max Payload is the maximum number of bytes supported by that
  prioritizer. If a prioritizer is configured with the option "Default Prioritizer: true," then all socket communications will use that default prioritizer.
  
  In the configuration below there are two classes defined under "NetworkPrioritizerService", one class "NetworkPrioritizerService2" defines en0, and en1.
  If en0 is down at any point, then en1 will be given priority before resorting to en2 and en3 of  "NetworkPrioritizerService3". If the throughput for 
  "NetworkPrioritizerService2" exceeds the defined throughput or the max payload of 1024, then "NetworkPrioritizerService3" will be used. If Max Payload and 
  Max Throughput are not defined, then they will not be limiting factors. 
  
  Since connection queues can't be re-prioritized, this can create a starvation problem if no connection is available. 
  The configuration is required to account for this.
    
   Controller Services:
   - name: NetworkPrioritizerService
     id: 2438e3c8-015a-1000-79ca-83af40ec1883
     class: NetworkPrioritizerService
     Properties:
         Linked Services: NetworkPrioritizerService2,NetworkPrioritizerService3
   - name: NetworkPrioritizerService2
     id: 2438e3c8-015a-1000-79ca-83af40ec1884
     class: NetworkPrioritizerService
     Properties:
         Network Controllers: en0,en1
         Max Throughput: 1,024,1024
         Max Payload: 1024
   - name: NetworkPrioritizerService3
     id: 2438e3c8-015a-1000-79ca-83af40ec1884
     class: NetworkPrioritizerService
     Properties:
         Network Controllers: en2,en3
         Max Throughput: 1,024,1024
         Max Payload: 1,024,1024

### Extension configuration
To notify the agent which extensions it should load see [Loading extensions](Extensions.md#Loading extensions).

### JNI Functionality
Please see the [JNI Configuration Guide](JNI.md).

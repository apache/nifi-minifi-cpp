<!--Licensed to the Apache Software Foundation (ASF) under one or morecontributor license agreements.  See the NOTICE file distributed withthis work for additional information regarding copyright ownership.The ASF licenses this file to You under the Apache License, Version 2.0(the "License"); you may not use this file except in compliance withthe License.  You may obtain a copy of the License at    http://www.apache.org/licenses/LICENSE-2.0Unless required by applicable law or agreed to in writing, softwaredistributed under the License is distributed on an "AS IS" BASIS,WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.See the License for the specific language governing permissions andlimitations under the License.-->
# Processors

## Table of Contents

- [AppendHostInfo](#appendhostinfo)
- [ApplyTemplate](#applytemplate)
- [BinFiles](#binfiles)
- [CapturePacket](#capturepacket)
- [CaptureRTSPFrame](#capturertspframe)
- [CompressContent](#compresscontent)
- [ConsumeMQTT](#consumemqtt)
- [ConvertHeartBeat](#convertheartbeat)
- [ConvertJSONAck](#convertjsonack)
- [ConvertUpdate](#convertupdate)
- [ExecuteProcess](#executeprocess)
- [ExecutePythonProcessor](#executepythonprocessor)
- [ExecuteSQL](#executesql)
- [ExecuteScript](#executescript)
- [ExtractText](#extracttext)
- [FetchSFTP](#fetchsftp)
- [FocusArchiveEntry](#focusarchiveentry)
- [GenerateFlowFile](#generateflowfile)
- [GetFile](#getfile)
- [GetGPS](#getgps)
- [GetTCP](#gettcp)
- [GetUSBCamera](#getusbcamera)
- [HashContent](#hashcontent)
- [InvokeHTTP](#invokehttp)
- [ListSFTP](#listsftp)
- [ListenHTTP](#listenhttp)
- [ListenSyslog](#listensyslog)
- [LogAttribute](#logattribute)
- [ManipulateArchive](#manipulatearchive)
- [MergeContent](#mergecontent)
- [PublishKafka](#publishkafka)
- [PublishMQTT](#publishmqtt)
- [PutFile](#putfile)
- [PutSFTP](#putsftp)
- [PutSQL](#putsql)
- [RouteOnAttribute](#routeonattribute)
- [TFApplyGraph](#tfapplygraph)
- [TFConvertImageToTensor](#tfconvertimagetotensor)
- [TFExtractTopLabels](#tfextracttoplabels)
- [TailFile](#tailfile)
- [UnfocusArchiveEntry](#unfocusarchiveentry)
- [UpdateAttribute](#updateattribute)
## AppendHostInfo

### Description 

Appends host information such as IP address and hostname as an attribute to incoming flowfiles.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Hostname Attribute|source.hostname||Flowfile attribute to used to record the agent's hostname|
|IP Attribute|source.ipv4||Flowfile attribute to used to record the agent's IP address|
|Network Interface Name|eth0||Network interface from which to read an IP v4 address|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## ApplyTemplate

### Description 

Applies the mustache template specified by the "Template" property and writes the output to the flow file content. FlowFile attributes are used as template parameters.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Template|||Path to the input mustache template file|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## BinFiles

### Description 

Bins flow files into buckets based on the number of entries or size of entries
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Max Bin Age|||The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>|
|Maximum Group Size|||The maximum size for the bundle. If not specified, there is no maximum.|
|Maximum Number of Entries|||The maximum number of files to include in a bundle. If not specified, there is no maximum.|
|Maximum number of Bins|100||Specifies the maximum number of bins that can be held in memory at any one time|
|Minimum Group Size|0||The minimum size of for the bundle|
|Minimum Number of Entries|1||The minimum number of files to include in a bundle|
### Properties 

| Name | Description |
| - | - |
|failure|If the bundle cannot be created, all FlowFiles that would have been used to created the bundle will be transferred to failure|
|original|The FlowFiles that were used to create the bundle|


## CapturePacket

### Description 

CapturePacket captures and writes one or more packets into a PCAP file that will be used as the content of a flow file. Configuration options exist to adjust the batching of PCAP files. PCAP batching will place a single PCAP into a flow file. A regular expression selects network interfaces. Bluetooth network interfaces can be selected through a separate option.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Base Directory|/tmp/||Scratch directory for PCAP files|
|Batch Size|50||The number of packets to combine within a given PCAP|
|Capture Bluetooth|false||True indicates that we support bluetooth interfaces|
|Network Controller|.*||Regular expression of the network controller(s) to which we will attach|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## CaptureRTSPFrame

### Description 

Captures a frame from the RTSP stream at specified intervals.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Image Encoding|.jpg||The encoding that should be applied the the frame images captured from the RTSP stream|
|RTSP Hostname|||Hostname of the RTSP stream we are trying to connect to|
|RTSP Password|||Password used to connect to the RTSP stream|
|RTSP Port|||Port that should be connected to to receive RTSP Frames|
|RTSP URI|||URI that should be appended to the RTSP stream hostname|
|RTSP Username|||The username for connecting to the RTSP stream|
### Properties 

| Name | Description |
| - | - |
|failure|Failures to capture RTSP frame|
|success|Successful capture of RTSP frame|


## CompressContent

### Description 

Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm and updates the mime.type attribute as appropriate
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Compression Format|use mime.type attribute||The compression format to use.|
|Compression Level|1||The compression level to use; this is valid only when using GZIP compression.|
|Mode|compress||Indicates whether the processor should compress content or decompress content.|
|Update Filename|false||Determines if filename extension need to be updated|
### Properties 

| Name | Description |
| - | - |
|failure|FlowFiles will be transferred to the failure relationship if they fail to compress/decompress|
|success|FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed|


## ConsumeMQTT

### Description 

This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. The the payload of the MQTT message becomes content of a FlowFile
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Broker URI|||The URI to use to connect to the MQTT broker|
|Client ID|||MQTT client ID to use|
|Connection Timeout|30 sec||Maximum time interval the client will wait for the network connection to the MQTT server|
|Keep Alive Interval|60 sec||Defines the maximum time interval between messages sent or received|
|Max Flow Segment Size|||Maximum flow content payload segment size for the MQTT record|
|Password|||Password to use when connecting to the broker|
|Quality of Service|MQTT_QOS_0||The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'|
|Queue Max Message|||Maximum number of messages allowed on the received MQTT queue|
|Session state|true||Whether to start afresh or resume previous flows. See the allowable value descriptions for more details|
|Topic|||The topic to publish the message to|
|Username|||Username to use when connecting to the broker|
### Properties 

| Name | Description |
| - | - |
|success|FlowFiles that are sent successfully to the destination are transferred to this relationship|


## ConvertHeartBeat

### Description 

ConvertHeartBeat converts heatrbeats into MQTT messages.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Listening Topic|||Name of topic to listen to|
|MQTT Controller Service|||Name of controller service that will be used for MQTT interactivity|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## ConvertJSONAck

### Description 

Converts JSON acks into an MQTT consumable by MQTTC2Protocol
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Listening Topic|||Name of topic to listen to|
|MQTT Controller Service|||Name of controller service that will be used for MQTT interactivity|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## ConvertUpdate

### Description 

onverts update messages into the appropriate RESTFul call
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Listening Topic|||Name of topic to listen to|
|MQTT Controller Service|||Name of controller service that will be used for MQTT interactivity|
|SSL Context Service|||The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## ExecuteProcess

### Description 

Runs an operating system command specified by the user and writes the output of that command to a FlowFile. If the command is expected to be long-running,the Processor can output the partial data on a specified interval. When this option is used, the output is expected to be in textual format,as it typically does not make sense to split binary data on arbitrary time-based intervals.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Batch Duration|0 sec||If the process is expected to be long-running and produce textual output, a batch duration can be specified.|
|Command|||Specifies the command to be executed; if just the name of an executable is provided, it must be in the user's environment PATH.<br/>**Supports Expression Language: true**|
|Command Arguments|||The arguments to supply to the executable delimited by white space. White space can be escaped by enclosing it in double-quotes.<br/>**Supports Expression Language: true**|
|Redirect Error Stream|false||If true will redirect any error stream output of the process to the output stream.|
|Working Directory|||The directory to use as the current working directory when executing the command<br/>**Supports Expression Language: true**|
### Properties 

| Name | Description |
| - | - |
|success|All created FlowFiles are routed to this relationship.|


## ExecutePythonProcessor

### Description 

Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the concurrent tasks to 1.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Module Directory|||Comma-separated list of paths to files and/or directories which
                                                 contain modules required by the script|
|Script File|||Path to script file to execute.
                                            Only one of Script File or Script Body may be used|
### Properties 

| Name | Description |
| - | - |
|failure|Script failures|
|success|Script successes|


## ExecuteSQL

### Description 

Execute provided SQL query. Query result rows will be outputted as new flow files with attribute keys equal to result column names and values equal to result values. There will be one output FlowFile per result row. This processor can be scheduled to run using the standard timer-based scheduling methods, or it can be triggered by an incoming FlowFile. If it is triggered by an incoming FlowFile, then attributes of that FlowFile will be available when evaluating the query.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Connection URL|||The database URL to connect to|
|SQL Statement|||The SQL statement to execute|
### Properties 

| Name | Description |
| - | - |
|failure|Failures which will not work if retried|
|original|The original FlowFile is sent here|
|success|After a successful SQL execution, result FlowFiles are sent here|


## ExecuteScript

### Description 

Executes a script given the flow file and a process session. The script is responsible for handling the incoming flow file (transfer to SUCCESS or remove, e.g.) as well as any flow files created by the script. If the handling is incomplete or incorrect, the session will be rolled back.Scripts must define an onTrigger function which accepts NiFi Context and Property objects. For efficiency, scripts are executed once when the processor is run, then the onTrigger method is called for each incoming flowfile. This enables scripts to keep state if they wish, although there will be a script context per concurrent task of the processor. In order to, e.g., compute an arithmetic sum based on incoming flow file information, set the concurrent tasks to 1.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Module Directory|||Comma-separated list of paths to files and/or directories which
                                                 contain modules required by the script|
|Script Body|||Body of script to execute.
                                            Only one of Script File or Script Body may be used|
|Script Engine|python||The engine to execute scripts (python, lua)|
|Script File|||Path to script file to execute.
                                            Only one of Script File or Script Body may be used|
### Properties 

| Name | Description |
| - | - |
|failure|Script failures|
|success|Script successes|


### Python Notes

- ExecuteScript uses native Python bindings, which means that any environment packages (e.g. those installed by pip) can be referenced.
- Virtual environments do work.
- It is possible to build MiNiFi - C++ against implementations which are compatible with CPython. CPython v2.7 as well as v3.4 do work, but only one version can be used at a time for a build.
- We recommend building multiple MiNiFi - C++ executables if there is a requirement for operating multiple Python versions or implmentations concurrently.
- Be mindful of the Global Interpreter Lock (GIL); While Python scripts will execute concurrently, execution is multiplexed from a single thread. Generally speaking, Python input/output and library calls with native implementations will release the GIL and enable concurrency, but long-running Python code such as loops will block other threads.

### Lua Notes

- Take care to use the `local` keyword where appropriate (such as inside of onTrigger functions and read/write callbacks) as scripts are long-running and this will help to avoid memory leaks.
- The following Lua packages are enabled/available for scripts:
  - `base`
  - `os`
  - `coroutine`
  - `math`
  - `io`
  - `string`
  - `table`
  - `utf8`
  - `package`

#### Python Example: Reading a File

```python
import codecs

class ReadCallback(object):
  def process(self, input_stream):
    content = codecs.getreader('utf-8')(input_stream).read()
    log.info('file content: %s' % content)
    return len(content)

def onTrigger(context, session):
  flow_file = session.get()

  if flow_file is not None:
    log.info('got flow file: %s' % flow_file.getAttribute('filename'))
    session.read(flow_file, ReadCallback())
    session.transfer(flow_file, REL_SUCCESS)
```

#### Python Example: Writing a File

```python
class WriteCallback(object):
  def process(self, output_stream):
    new_content = 'hello 2'.encode('utf-8')
    output_stream.write(new_content)
    return len(new_content)

def onTrigger(context, session):
  flow_file = session.get()
  if flow_file is not None:
    log.info('got flow file: %s' % flow_file.getAttribute('filename'))
    session.write(flow_file, WriteCallback())
    session.transfer(flow_file, REL_SUCCESS)
```

#### Lua Example: Reading a File

```lua
read_callback = {}

function read_callback.process(self, input_stream)
    local content = input_stream:read()
    log:info('file content: ' .. content)
    return #content
end

function onTrigger(context, session)
  local flow_file = session:get()

  if flow_file ~= nil then
    log:info('got flow file: ' .. flow_file:getAttribute('filename'))
    session:read(flow_file, read_callback)
    session:transfer(flow_file, REL_SUCCESS)
  end
end
```

#### Lua Example: Writing a File

```lua
write_callback = {}

function write_callback.process(self, output_stream)
  local new_content = 'hello 2'
  output_stream:write(new_content)
  return #new_content
end

function onTrigger(context, session)
  local flow_file = session:get()

  if flow_file ~= nil then
    log:info('got flow file: ' .. flow_file:getAttribute('filename'))
    session:write(flow_file, write_callback)
    session:transfer(flow_file, REL_SUCCESS)
  end
end
```

## ExtractText

### Description 

Extracts the content of a FlowFile and places it into an attribute.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Attribute|||Attribute to set from content|
|Enable Case-insensitive Matching|false||Indicates that two characters match even if they are in a different case. |
|Enable repeating capture group|false||f set to true, every string matching the capture groups will be extracted. Otherwise, if the Regular Expression matches more than once, only the first match will be extracted.|
|Include Capture Group 0|true||Indicates that Capture Group 0 should be included as an attribute. Capture Group 0 represents the entirety of the regular expression match, is typically not used, and could have considerable length.|
|Maximum Capture Group Length|1024||Specifies the maximum number of characters a given capture group value can have. Any characters beyond the max will be truncated.|
|Regex Mode|false||Set this to extract parts of flowfile content using regular experssions in dynamic properties|
|Size Limit|2097152||Maximum number of bytes to read into the attribute. 0 for no limit. Default is 2MB.|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## FetchSFTP

### Description 

Fetches the content of a file from a remote SFTP server and overwrites the contents of an incoming FlowFile with the content of the remote file.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|**Completion Strategy**|None|Delete File<br>Move File<br>None<br>|Specifies what to do with the original file on the server once it has been pulled into NiFi. If the Completion Strategy fails, a warning will be logged but the data will still be transferred.|
|**Connection Timeout**|30 sec||Amount of time to wait before timing out while creating a connection|
|**Create Directory**|false||Specifies whether or not the remote directory should be created if it does not exist.|
|**Data Timeout**|30 sec||When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems|
|Disable Directory Listing|false||Control how 'Move Destination Directory' is created when 'Completion Strategy' is 'Move File' and 'Create Directory' is enabled. If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists.|
|Host Key File|||If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used|
|**Hostname**|||The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**|
|Http Proxy Password|||Http Proxy Password<br/>**Supports Expression Language: true**|
|Http Proxy Username|||Http Proxy Username<br/>**Supports Expression Language: true**|
|Move Destination Directory|||The directory on the remote server to move the original file to once it has been ingested into NiFi. This property is ignored unless the Completion Strategy is set to 'Move File'. The specified directory must already exist on the remote system if 'Create Directory' is disabled, or the rename will fail.<br/>**Supports Expression Language: true**|
|Password|||Password for the user account<br/>**Supports Expression Language: true**|
|**Port**|||The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**|
|Private Key Passphrase|||Password for the private key<br/>**Supports Expression Language: true**|
|Private Key Path|||The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**|
|Proxy Host|||The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Port|||The port of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Type|DIRECT|DIRECT<br>HTTP<br>SOCKS<br>|Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN|
|**Remote File**|||The fully qualified filename on the remote system<br/>**Supports Expression Language: true**|
|**Send Keep Alive On Timeout**|true||Indicates whether or not to send a single Keep Alive message when SSH socket times out|
|**Strict Host Key Checking**|false||Indicates whether or not strict enforcement of hosts keys should be applied|
|**Use Compression**|false||Indicates whether or not ZLIB compression should be used when transferring files|
|**Username**|||Username<br/>**Supports Expression Language: true**|
### Properties 

| Name | Description |
| - | - |
|comms.failure|Any FlowFile that could not be fetched from the remote server due to a communications failure will be transferred to this Relationship.|
|not.found|Any FlowFile for which we receive a 'Not Found' message from the remote server will be transferred to this Relationship.|
|permission.denied|Any FlowFile that could not be fetched from the remote server due to insufficient permissions will be transferred to this Relationship.|
|success|All FlowFiles that are received are routed to success|


## FocusArchiveEntry

### Description 

Allows manipulation of entries within an archive (e.g. TAR) by focusing on one entry within the archive at a time. When an archive entry is focused, that entry is treated as the content of the FlowFile and may be manipulated independently of the rest of the archive. To restore the FlowFile to its original state, use UnfocusArchiveEntry.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Path|||The path within the archive to focus ("/" to focus the total archive)|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## GenerateFlowFile

### Description 

This processor creates FlowFiles with random data or custom content. GenerateFlowFile is useful for load testing, configuration, and simulation.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Batch Size|1||The number of FlowFiles to be transferred in each invocation|
|Data Format|Binary|Text<br>Binary<br>|Specifies whether the data should be Text or Binary|
|File Size|1 kB||The size of the file that will be used|
|Unique FlowFiles|true||If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## GetFile

### Description 

Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Batch Size|10||The maximum number of files to pull in each iteration|
|File Filter|[^\.].*||Only files whose names match the given regular expression will be picked up|
|Ignore Hidden Files|true||Indicates whether or not hidden files should be ignored|
|**Input Directory**|.||The input directory from which to pull files<br/>**Supports Expression Language: true**|
|Keep Source File|false||If true, the file is not deleted after it has been copied to the Content Repository|
|Maximum File Age|0 sec||The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored|
|Minimum File Age|0 sec||The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored|
|Minimum File Size|0 B||The maximum size that a file can be in order to be pulled|
|Polling Interval|0 sec||Indicates how long to wait before performing a directory listing|
|Recurse Subdirectories|true||Indicates whether or not to pull files from subdirectories|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## GetGPS

### Description 

Obtains GPS coordinates from the GPSDHost and port.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|GPSD Host|localhost||The host running the GPSD daemon|
|GPSD Port|2947||The GPSD daemon port|
|GPSD Wait Time|50000000||Timeout value for waiting for data from the GPSD instance|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## GetTCP

### Description 

Establishes a TCP Server that defines and retrieves one or more byte messages from clients
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|SSL Context Service|||SSL Context Service Name|
|Stay Connected|true||Determines if we keep the same socket despite having no data|
|concurrent-handler-count|1||Number of concurrent handlers for this session|
|connection-attempt-timeout|3||Maximum number of connection attempts before attempting backup hosts, if configured|
|end-of-message-byte|13||Byte value which denotes end of message. Must be specified as integer within the valid byte range  (-128 thru 127). For example, '13' = Carriage return and '10' = New line. Default '13'.|
|**endpoint-list**|||A comma delimited list of the endpoints to connect to. The format should be <server_address>:<port>.|
|receive-buffer-size|16 MB||The size of the buffer to receive data in. Default 16384 (16MB).|
### Properties 

| Name | Description |
| - | - |
|partial|Indicates an incomplete message as a result of encountering the end of message byte trigger|
|success|All files are routed to success|


## GetUSBCamera

### Description 

Gets images from USB Video Class (UVC)-compatible devices. Outputs one flow file per frame at the rate specified by the FPS property in the format specified by the Format property.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|FPS|1||Frames per second to capture from USB camera|
|Format|PNG||Frame format (currently only PNG and RAW are supported; RAW is a binary pixel buffer of RGB values)|
|Height|||Target height of image to capture from USB camera|
|USB Product ID|0x0||USB Product ID of camera device, in hexadecimal format|
|USB Serial No.|||USB Serial No. of camera device|
|USB Vendor ID|0x0||USB Vendor ID of camera device, in hexadecimal format|
|Width|||Target width of image to capture from USB camera|
### Properties 

| Name | Description |
| - | - |
|failure|Failures sent here|
|success|Sucessfully captured images sent here|


## HashContent

### Description 

HashContent calculates the checksum of the content of the flowfile and adds it as an attribute. Configuration options exist to select hashing algorithm and set the name of the attribute.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Hash Algorithm|SHA256||Name of the algorithm used to generate checksum|
|Hash Attribute|Checksum||Attribute to store checksum to|
### Properties 

| Name | Description |
| - | - |
|failure|failure operational on the flow record|
|success|success operational on the flow record|


## InvokeHTTP

### Description 

An HTTP client processor which can interact with a configurable HTTP Endpoint. The destination URL and HTTP Method are configurable. FlowFile attributes are converted to HTTP headers and the FlowFile contents are included as the body of the request (if the HTTP Method is PUT, POST or PATCH).
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Always Output Response|false||Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is |
|Attributes to Send|||Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers.|
|Connection Timeout|5 secs||Max wait time for connection to remote service.|
|Content-type|application/octet-stream||The Content-Type to specify for when content is being transmitted through a PUT, POST or PATCH. In the case of an empty value after evaluating an expression language expression, Content-Type defaults to|
|Disable Peer Verification|false||Disables peer verification for the SSL session|
|HTTP Method|GET||HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body.|
|Include Date Header|true||Include an RFC-2616 Date header in the request.|
|Proxy Host|||The fully qualified hostname or IP address of the proxy server|
|Proxy Port|||The port of the proxy server|
|Read Timeout|15 secs||Max wait time for response from remote service.|
|Remote URL|||Remote URL which will be connected to, including scheme, host, port, path.<br/>**Supports Expression Language: true**|
|SSL Context Service|||The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections.|
|Use Chunked Encoding|false||When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header and instead send 'Transfer-Encoding' with a value of 'chunked'. This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks.|
|invokehttp-proxy-password|||Password to set when authenticating against proxy|
|invokehttp-proxy-username|||Username to set when authenticating against proxy|
|send-message-body|true||If true, sends the HTTP message body on POST/PUT/PATCH requests (default).  If false, suppresses the message body and content-type header for these requests.|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## ListSFTP

### Description 

Performs a listing of the files residing on an SFTP server. For each file that is found on the remote server, a new FlowFile will be created with the filename attribute set to the name of the file on the remote server. This can then be used in conjunction with FetchSFTP in order to fetch those files.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|**Connection Timeout**|30 sec||Amount of time to wait before timing out while creating a connection|
|**Data Timeout**|30 sec||When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems|
|Entity Tracking Initial Listing Target|All Available|All Available<br>Tracking Time Window<br>|Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.|
|Entity Tracking Time Window|||Specify how long this processor should track already-listed entities. 'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: 1. does not exist in the already-listed entities, 2. has newer timestamp than the cached entity, 3. has different size than the cached entity. If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. Used by 'Tracking Entities' strategy.|
|File Filter Regex|||Provides a Java Regular Expression for filtering Filenames; if a filter is supplied, only files whose names match that Regular Expression will be fetched|
|**Follow symlink**|false||If true, will pull even symbolic files and also nested symbolic subdirectories; otherwise, will not read symbolic files and will not traverse symbolic link subdirectories|
|Host Key File|||If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used|
|**Hostname**|||The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**|
|Http Proxy Password|||Http Proxy Password<br/>**Supports Expression Language: true**|
|Http Proxy Username|||Http Proxy Username<br/>**Supports Expression Language: true**|
|**Ignore Dotted Files**|true||If true, files whose names begin with a dot (".") will be ignored|
|**Listing Strategy**|Tracking Timestamps|Tracking Entities<br>Tracking Timestamps<br>|Specify how to determine new/updated entities. See each strategy descriptions for detail.|
|Maximum File Age|||The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored|
|Maximum File Size|||The maximum size that a file must be in order to be pulled|
|**Minimum File Age**|0 sec||The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored|
|**Minimum File Size**|0 B||The minimum size that a file must be in order to be pulled|
|Password|||Password for the user account<br/>**Supports Expression Language: true**|
|Path Filter Regex|||When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned|
|**Port**|||The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**|
|Private Key Passphrase|||Password for the private key<br/>**Supports Expression Language: true**|
|Private Key Path|||The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**|
|Proxy Host|||The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Port|||The port of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Type|DIRECT|DIRECT<br>HTTP<br>SOCKS<br>|Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN|
|Remote Path|||The fully qualified filename on the remote system<br/>**Supports Expression Language: true**|
|**Search Recursively**|false||If true, will pull files from arbitrarily nested subdirectories; otherwise, will not traverse subdirectories|
|**Send Keep Alive On Timeout**|true||Indicates whether or not to send a single Keep Alive message when SSH socket times out|
|**State File**|ListSFTP||Specifies the file that should be used for storing state about what data has been ingested so that upon restart MiNiFi can resume from where it left off|
|**Strict Host Key Checking**|false||Indicates whether or not strict enforcement of hosts keys should be applied|
|**Target System Timestamp Precision**|Auto Detect|Auto Detect<br>Milliseconds<br>Minutes<br>Seconds<br>|Specify timestamp precision at the target system. Since this processor uses timestamp of entities to decide which should be listed, it is crucial to use the right timestamp precision.|
|**Username**|||Username<br/>**Supports Expression Language: true**|
### Properties 

| Name | Description |
| - | - |
|success|All FlowFiles that are received are routed to success|


## ListenHTTP

### Description 

Starts an HTTP Server and listens on a given base path to transform incoming requests into FlowFiles. The default URI of the Service will be http://{hostname}:{port}/contentListener. Only HEAD, POST, and GET requests are supported. PUT, and DELETE will result in an error and the HTTP response status code 405. The response body text for all requests, by default, is empty (length of 0). A static response body can be set for a given URI by sending input files to ListenHTTP with the http.type attribute set to response_body. The response body FlowFile filename attribute is appended to the Base Path property (separated by a /) when mapped to incoming requests. The mime.type attribute of the response body FlowFile is used for the Content-type header in responses. Response body content can be cleared by sending an empty (size 0) FlowFile for a given URI mapping.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Authorized DN Pattern|.*||A Regular Expression to apply against the Distinguished Name of incoming connections. If the Pattern does not match the DN, the connection will be refused.|
|Base Path|contentListener||Base path for incoming connections|
|HTTP Headers to receive as Attributes (Regex)|||Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes|
|**Listening Port**|80||The Port to listen on for incoming connections. 0 means port is going to be selected randomly.|
|SSL Certificate|||File containing PEM-formatted file including TLS/SSL certificate and key|
|SSL Certificate Authority|||File containing trusted PEM-formatted certificates|
|SSL Minimum Version|SSL2|SSL2<br>SSL3<br>TLS1.0<br>TLS1.1<br>TLS1.2<br>|Minimum TLS/SSL version allowed (SSL2, SSL3, TLS1.0, TLS1.1, TLS1.2)|
|SSL Verify Peer|no|no<br>yes<br>|Whether or not to verify the client's certificate (yes/no)|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## ListenSyslog

### Description 

Listens for Syslog messages being sent to a given port over TCP or UDP. Incoming messages are checked against regular expressions for RFC5424 and RFC3164 formatted messages. The format of each message is: (<PRIORITY>)(VERSION )(TIMESTAMP) (HOSTNAME) (BODY) where version is optional. The timestamp can be an RFC5424 timestamp with a format of "yyyy-MM-dd'T'HH:mm:ss.SZ" or "yyyy-MM-dd'T'HH:mm:ss.S+hh:mm", or it can be an RFC3164 timestamp with a format of "MMM d HH:mm:ss". If an incoming messages matches one of these patterns, the message will be parsed and the individual pieces will be placed in FlowFile attributes, with the original message in the content of the FlowFile. If an incoming message does not match one of these patterns it will not be parsed and the syslog.valid attribute will be set to false with the original message in the content of the FlowFile. Valid messages will be transferred on the success relationship, and invalid messages will be transferred on the invalid relationship.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Max Batch Size|1||The maximum number of Syslog events to add to a single FlowFile.|
|Max Number of TCP Connections|2||The maximum number of concurrent connections to accept Syslog messages in TCP mode.|
|Max Size of Socket Buffer|1 MB||The maximum size of the socket buffer that should be used.|
|Message Delimiter|\n||Specifies the delimiter to place between Syslog messages when multiple messages are bundled together (see <Max Batch Size> core::Property).|
|Parse Messages|false||Indicates if the processor should parse the Syslog messages. If set to false, each outgoing FlowFile will only.|
|Port|514||The port for Syslog communication|
|Protocol|UDP|UDP<br>TCP<br>|The protocol for Syslog communication.|
|Receive Buffer Size|65507 B||The size of each buffer used to receive Syslog messages.|
### Properties 

| Name | Description |
| - | - |
|invalid|SysLog message format invalid|
|success|All files are routed to success|


## LogAttribute

### Description 

Logs attributes of flow files in the MiNiFi application log.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Attributes to Ignore|||A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.|
|Attributes to Log|||A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.|
|FlowFiles To Log|1||Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.|
|Log Level||debug<br>error<br>info<br>trace<br>warn<br>|The Log Level to use when logging the Attributes|
|Log Payload|false||If true, the FlowFile's payload will be logged, in addition to its attributes.otherwise, just the Attributes will be logged|
|Log Prefix|||Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.|
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## ManipulateArchive

### Description 

Performs an operation which manipulates an archive without needing to split the archive into multiple FlowFiles.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|After|||For operations which result in new entries, places the new entry after the entry specified by this property.|
|Before|||For operations which result in new entries, places the new entry before the entry specified by this property.|
|Destination|||Destination for operations (touch, move or copy) which result in new entries.|
|Operation|||Operation to perform on the archive (touch, remove, copy, move).|
|Target|||An existing entry within the archive to perform the operation on.|
### Properties 

| Name | Description |
| - | - |
|failure|FlowFiles will be transferred to the failure relationship if the operation fails.|
|success|FlowFiles will be transferred to the success relationship if the operation succeeds.|


## MergeContent

### Description 

Merges a Group of FlowFiles together based on a user-defined strategy and packages them into a single FlowFile. MergeContent should be configured with only one incoming connection as it won't create grouped Flow Files.This processor updates the mime.type attribute as appropriate.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Correlation Attribute Name|||Correlation Attribute Name|
|Delimiter Strategy|Filename||Determines if Header, Footer, and Demarcator should point to files|
|Demarcator File|||Filename specifying the demarcator to use|
|Footer File|||Filename specifying the footer to use|
|Header File|||Filename specifying the header to use|
|Keep Path|false||If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry|
|Max Bin Age|||The maximum age of a Bin that will trigger a Bin to be complete. Expected format is <duration> <time unit>|
|Maximum Group Size|||The maximum size for the bundle. If not specified, there is no maximum.|
|Maximum Number of Entries|||The maximum number of files to include in a bundle. If not specified, there is no maximum.|
|Maximum number of Bins|100||Specifies the maximum number of bins that can be held in memory at any one time|
|Merge Format|Binary Concatenation||Merge Format|
|Merge Strategy|Defragment||Defragment or Bin-Packing Algorithm|
|Minimum Group Size|0||The minimum size of for the bundle|
|Minimum Number of Entries|1||The minimum number of files to include in a bundle|
### Properties 

| Name | Description |
| - | - |
|failure|If the bundle cannot be created, all FlowFiles that would have been used to created the bundle will be transferred to failure|
|merged|The FlowFile containing the merged content|
|original|The FlowFiles that were used to create the bundle|


## PublishKafka

### Description 

This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The content of a FlowFile becomes the contents of a Kafka message. This message is optionally assigned a key by using the <Kafka Key> Property.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Attributes to Send as Headers|||Any attribute whose name matches the regex will be added to the Kafka messages as a Header|
|Batch Size|||Maximum number of messages batched in one MessageSet|
|**Client Name**|||Client Name to use when communicating with Kafka<br/>**Supports Expression Language: true**|
|Compress Codec|none||compression codec to use for compressing message sets|
|Delivery Guarantee|DELIVERY_ONE_NODE||TSpecifies the requirement for guaranteeing that a message is sent to Kafka<br/>**Supports Expression Language: true**|
|Kerberos Keytab Path|||The path to the location on the local filesystem where the kerberos keytab is located. Read permission on the file is required.|
|Kerberos Principal|||Keberos Principal|
|Kerberos Service Name|||Kerberos Service Name|
|**Known Brokers**|||A comma-separated list of known Kafka Brokers in the format <host>:<port><br/>**Supports Expression Language: true**|
|Max Flow Segment Size|||Maximum flow content payload segment size for the kafka record|
|Max Request Size|||Maximum Kafka protocol request message size|
|Message Key Field|||The name of a field in the Input Records that should be used as the Key for the Kafka message.
Supports Expression Language: true (will be evaluated using flow file attributes)|
|Queue Buffering Max Time|||Delay to wait for messages in the producer queue to accumulate before constructing message batches|
|Queue Max Buffer Size|||Maximum total message size sum allowed on the producer queue|
|Queue Max Message|||Maximum number of messages allowed on the producer queue|
|Request Timeout|||The ack timeout of the producer request in milliseconds<br/>**Supports Expression Language: true**|
|Security CA|||File or directory path to CA certificate(s) for verifying the broker's key|
|Security Cert|||Path to client's public key (PEM) used for authentication|
|Security Pass Phrase|||Private key passphrase|
|Security Private Key|||Path to client's private key (PEM) used for authentication|
|Security Protocol|||Protocol used to communicate with brokers|
|**Topic Name**|||The Kafka Topic of interest<br/>**Supports Expression Language: true**|
### Properties 

| Name | Description |
| - | - |
|failure|Any FlowFile that cannot be sent to Kafka will be routed to this Relationship|
|success|Any FlowFile that is successfully sent to Kafka will be routed to this Relationship|


## PublishMQTT

### Description 

PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Broker URI|||The URI to use to connect to the MQTT broker|
|Client ID|||MQTT client ID to use|
|Connection Timeout|30 sec||Maximum time interval the client will wait for the network connection to the MQTT server|
|Keep Alive Interval|60 sec||Defines the maximum time interval between messages sent or received|
|Max Flow Segment Size|||Maximum flow content payload segment size for the MQTT record|
|Password|||Password to use when connecting to the broker|
|Quality of Service|MQTT_QOS_0||The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'|
|Retain|false||Retain MQTT published record in broker|
|Session state|true||Whether to start afresh or resume previous flows. See the allowable value descriptions for more details|
|Topic|||The topic to publish the message to|
|Username|||Username to use when connecting to the broker|
### Properties 

| Name | Description |
| - | - |
|failure|FlowFiles that failed to send to the destination are transferred to this relationship|
|success|FlowFiles that are sent successfully to the destination are transferred to this relationship|


## PutFile

### Description 

Writes the contents of a FlowFile to the local file system
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Conflict Resolution Strategy|fail|fail<br>ignore<br>replace<br>|Indicates what should happen when a file with the same name already exists in the output directory|
|**Create Missing Directories**|true||If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.|
|Directory|.||The output directory to which to put files<br/>**Supports Expression Language: true**|
|Maximum File Count|-1||Specifies the maximum number of files that can exist in the output directory|
### Properties 

| Name | Description |
| - | - |
|failure|Failed files (conflict, write failure, etc.) are transferred to failure|
|success|All files are routed to success|


## PutSFTP

### Description 

Sends FlowFiles to an SFTP Server
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|**Batch Size**|500||The maximum number of FlowFiles to send in a single connection|
|**Conflict Resolution**|NONE|FAIL<br>IGNORE<br>NONE<br>REJECT<br>RENAME<br>REPLACE<br>|Determines how to handle the problem of filename collisions|
|**Connection Timeout**|30 sec||Amount of time to wait before timing out while creating a connection|
|**Create Directory**|false||Specifies whether or not the remote directory should be created if it does not exist.|
|**Data Timeout**|30 sec||When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems|
|Disable Directory Listing|false||If set to 'true', directory listing is not performed prior to create missing directories. By default, this processor executes a directory listing command to see target directory existence before creating missing directories. However, there are situations that you might need to disable the directory listing such as the following. Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, then an error is returned because the directory already exists.|
|Dot Rename|true||If true, then the filename of the sent file is prepended with a "." and then renamed back to the original once the file is completely sent. Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.|
|Host Key File|||If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used|
|**Hostname**|||The fully qualified hostname or IP address of the remote system<br/>**Supports Expression Language: true**|
|Http Proxy Password|||Http Proxy Password<br/>**Supports Expression Language: true**|
|Http Proxy Username|||Http Proxy Username<br/>**Supports Expression Language: true**|
|Last Modified Time|||The lastModifiedTime to assign to the file after transferring it. If not set, the lastModifiedTime will not be changed. Format must be yyyy-MM-dd'T'HH:mm:ssZ. You may also use expression language such as ${file.lastModifiedTime}. If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.<br/>**Supports Expression Language: true**|
|Password|||Password for the user account<br/>**Supports Expression Language: true**|
|Permissions|||The permissions to assign to the file after transferring it. Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). If not set, the permissions will not be changed. You may also use expression language such as ${file.permissions}. If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.<br/>**Supports Expression Language: true**|
|**Port**|||The port that the remote system is listening on for file transfers<br/>**Supports Expression Language: true**|
|Private Key Passphrase|||Password for the private key<br/>**Supports Expression Language: true**|
|Private Key Path|||The fully qualified path to the Private Key file<br/>**Supports Expression Language: true**|
|Proxy Host|||The fully qualified hostname or IP address of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Port|||The port of the proxy server<br/>**Supports Expression Language: true**|
|Proxy Type|DIRECT|DIRECT<br>HTTP<br>SOCKS<br>|Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. Supported proxies: HTTP + AuthN, SOCKS + AuthN|
|Reject Zero-Byte Files|true||Determines whether or not Zero-byte files should be rejected without attempting to transfer|
|Remote Group|||Integer value representing the Group ID to set on the file after transferring it. If not set, the group will not be set. You may also use expression language such as ${file.group}. If the value is invalid, the processor will not be invalid but will fail to change the group of the file.<br/>**Supports Expression Language: true**|
|Remote Owner|||Integer value representing the User ID to set on the file after transferring it. If not set, the owner will not be set. You may also use expression language such as ${file.owner}. If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.<br/>**Supports Expression Language: true**|
|Remote Path|||The path on the remote system from which to pull or push files<br/>**Supports Expression Language: true**|
|**Send Keep Alive On Timeout**|true||Indicates whether or not to send a single Keep Alive message when SSH socket times out|
|**Strict Host Key Checking**|false||Indicates whether or not strict enforcement of hosts keys should be applied|
|Temporary Filename|||If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. If this value is set, the Dot Rename property is ignored.<br/>**Supports Expression Language: true**|
|**Use Compression**|false||Indicates whether or not ZLIB compression should be used when transferring files|
|**Username**|||Username<br/>**Supports Expression Language: true**|
### Properties 

| Name | Description |
| - | - |
|failure|FlowFiles that failed to send to the remote system; failure is usually looped back to this processor|
|reject|FlowFiles that were rejected by the destination system|
|success|FlowFiles that are successfully sent will be routed to success|


## PutSQL

### Description 

Executes a SQL UPDATE or INSERT command. The content of an incoming FlowFile is expected to be the SQL command to execute. The SQL command may use the ? character to bind parameters. In this case, the parameters to use must exist as FlowFile attributes with the naming convention sql.args.N.type and sql.args.N.value, where N is a positive integer. The content of the FlowFile is expected to be in UTF-8 format.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|Batch Size|1||The maximum number of flow files to process in one batch|
|Connection URL|||The database URL to connect to|
|SQL Statement|||The SQL statement to execute|
### Properties 

| Name | Description |
| - | - |
|failure|Failures which will not work if retried|
|retry|Failures which might work if retried|
|success|After a successful put SQL operation, FlowFiles are sent here|


## RouteOnAttribute

### Description 

Routes FlowFiles based on their Attributes using the Attribute Expression Language.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
### Properties 

| Name | Description |
| - | - |
|failure|Failed files are transferred to failure|
|unmatched|Files which do not match any expression are routed here|


## TFApplyGraph 

### Description

Applies a TensorFlow graph to the tensor protobuf supplied as input. The tensor
is fed into the node specified by the `Input Node` property. The output
FlowFile is a tensor protobuf extracted from the node specified by the `Output
Node` property.

TensorFlow graphs are read dynamically by feeding a graph protobuf to the
processor with the `tf.type` property set to `graph`.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Input Node** | | | The node of the TensorFlow graph to feed tensor inputs to |
| **Output Node** | | | The node of the TensorFlow graph to read tensor outputs from |

### Relationships

| Name | Description |
| - | - |
| success | Successful graph application outputs as tensor protobufs |
| retry | Inputs which fail graph application but may work if sent again |
| failure | Failures which will not work if retried |
## TFConvertImageToTensor

### Description

Converts the input image file into a tensor protobuf. The image will be resized
to the given output tensor dimensions.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Input Format** | | PNG, RAW | The format of the input image (PNG or RAW). RAW is RGB24. |
| **Input Width** | | | The width, in pixels, of the input image. |
| **Input Height** | | | The height, in pixels, of the input image. |
| Crop Offset X | | | The X (horizontal) offset, in pixels, to crop the input image (relative to top-left corner). |
| Crop Offset Y | | | The Y (vertical) offset, in pixels, to crop the input image (relative to top-left corner). |
| Crop Size X | | | The X (horizontal) size, in pixels, to crop the input image. |
| Crop Size Y | | | The Y (vertical) size, in pixels, to crop the input image. |
| **Output Width** | | | The width, in pixels, of the output image. |
| **Output Height** | | | The height, in pixels, of the output image. |
| **Channels** | 3 | | The number of channels (e.g. 3 for RGB, 4 for RGBA) in the input image. |

### Relationships

| Name | Description |
| - | - |
| success | Successfully read tensor protobufs |
| failure | Inputs which could not be converted to tensor protobufs |
## TFExtractTopLabels

### Description

Extracts the top 5 labels for categorical inference models.

Labels are fed as newline (`\n`) -delimited files where each line is a label
for the tensor index equivalent to the line number. Label files must be fed in
with the `tf.type` property set to `labels`.

The top 5 labels are written to the following attributes:

- `top_label_0`
- `top_label_1`
- `top_label_2`
- `top_label_3`
- `top_label_4`

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |

### Relationships

| Name | Description |
| - | - |
| success | Successful FlowFiles are sent here with labels as attributes |
| retry | Failures which might work if retried |
| failure | Failures which will not work if retried |
## TailFile

### Description 

"Tails" a file, or a list of files, ingesting data from the file as it is written to the file. The file is expected to be textual. Data is ingested only when a new line is encountered (carriage return or new-line character or combination). If the file to tail is periodically "rolled over", as is generally the case with log files, an optional Rolling Filename Pattern can be used to retrieve data from files that have rolled over, even if the rollover occurred while NiFi was not running (provided that the data still exists upon restart of NiFi). It is generally advisable to set the Run Schedule to a few seconds, rather than running with the default value of 0 secs, as this Processor will consume a lot of resources if scheduled very aggressively. At this time, this Processor does not support ingesting files that have been compressed when 'rolled over'.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
|File to Tail|||Fully-qualified filename of the file that should be tailed when using single file mode, or a file regex when using multifile mode|
|Input Delimiter|||Specifies the character that should be used for delimiting the data being tailedfrom the incoming file.If none is specified, data will be ingested as it becomes available.|
|State File|TailFileState||Specifies the file that should be used for storing state about what data has been ingested so that upon restart NiFi can resume from where it left off|
|tail-base-directory||||
|**tail-mode**|Single file|Single file<br>Multiple file<br>|Specifies the tail file mode. In 'Single file' mode only a single file will be watched. In 'Multiple file' mode a regex may be used. Note that in multiple file mode we will still continue to watch for rollover on the initial set of watched files. The Regex used to locate multiple files will be run during the schedule phrase. Note that if rotated files are matched by the regex, those files will be tailed.|
### Properties 

| Name | Description |
| - | - |
|success|All files are routed to success|


## UnfocusArchiveEntry

### Description 

Restores a FlowFile which has had an archive entry focused via FocusArchiveEntry to its original state.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
### Properties 

| Name | Description |
| - | - |
|success|success operational on the flow record|


## UpdateAttribute

### Description 

This processor updates the attributes of a FlowFile using properties that are added by the user. This allows you to set default attribute changes that affect every FlowFile going through the processor, equivalent to the "basic" usage in Apache NiFi.
### Properties 

In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description | 
| - | - | - | - | 
### Properties 

| Name | Description |
| - | - |
|failure|Failed files are transferred to failure|
|success|All files are routed to success|



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

# Processors

## Table of Contents

- [AppendHostInfo](#appendhostinfo)
- [ApplyTemplate](#applytemplate)
- [CapturePacket](#capturepacket)
- [CompressContent](#compresscontent)
- [ConsumeMQTT](#consumemqtt)
- [ConvertHeartBeat](#convertheartbeat)
- [ConvertJSONAck](#convertjsonack)
- [ConvertUpdate](#convertupdate)
- [ExecuteProcess](#executeprocess)
- [ExecuteScript](#executescript)
- [ExecuteSQL](#executesql)
- [ExtractText](#extracttext)
- [FocusArchiveEntry](#focusarchiveentry)
- [GenerateFlowFile](#generateflowfile)
- [GetFile](#getfile)
- [GetUSBCamera](#getusbcamera)
- [GetTCP](#gettcp)
- [HashContent](#hashcontent)
- [InvokeHTTP](#invokehttp)
- [ListenHTTP](#listenhttp)
- [ListenSyslog](#listensyslog)
- [LogAttribute](#logattribute)
- [ManipulateArchive](#manipulatearchive)
- [MergeContent](#mergecontent)
- [PublishKafka](#publishkafka)
- [PublishMQTT](#publishmqtt)
- [PutFile](#putfile)
- [PutSQL](#putsql)
- [RouteOnAttribute](#routeonattribute)
- [TailFile](#tailfile)
- [TFApplyGraph](#tfapplygraph)
- [TFConvertImageToTensor](#tfconvertimagetotensor)
- [TFExtractTopLabels](#tfextracttoplabels)
- [UnfocusArchiveEntry](#unfocusarchiveentry)
- [UpdateAttribute](#updateattribute)

## AppendHostInfo

### Description

Appends host information such as IP address and hostname  as an attribute to
incoming flowfiles.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Network Interface Name** | eth0 | | Network interface from which to read an IP v4 address |
| **Hostname Attribute** | source.hostname |  | Flowfile attribute to used to record the agent's hostname |
| **IP Attribute** | source.ipv4 |  | Flowfile attribute to used to record the agent's IP address |

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this relationship. |

## ApplyTemplate

### Description

Applies the mustache template specified by the "Template" property and writes
the output to the flow file content. FlowFile attributes are used as template
parameters.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Template** | | | Path to the input mustache template file<br>**Supports Expression Language: true** |

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this relationship. |


## CapturePacket

### Description

CapturePacket captures and writes one or more packets into a PCAP file that will be used as the content
of a flow file. Configuration options exist to adjust the batching of PCAP files. PCAP batching will 
place a single PCAP into a flow file. A regular expression selects network interfaces. Bluetooth 
network interfaces can be selected through a separate option. 

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| Base Directory | /tmp/ | | A base directory where pcap files are stored temporarily |
| Batch Size | 50 |  | Number of captured packets written into a PCAP file |
| Capture Bluetooth | false |  | Captures bluetooth interfaces if true  |
| Network Controller | .* |  | Regular expression of the network controller(s) to which packet capture will be attached|

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this relationship. |


## HashContent

### Description

HashContent calculates the checksum of the content of the flowfile and adds it as an attribute.
Configuration options exist to select hashing algorithm and set the name of the attribute. 

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| Hash Attribute | Checksum | | Name of the attribute the processor will use to add the checksum |
| Hash Algorithm | MD5 | MD5, SHA1, SHA256 | Name of the algorithm used to calculate the checksum |
| Fail on empty | false | false, true | Route flow files with empty content to failure relationship |

### Relationships

| Name | Description |
| - | - |
| success | By default all flow files are routed to this relationship. |
| failure | In case "Fail on empty" property is set to true, flow files with empty content are routed to this relationship. |

## ConvertHeartBeat

This Processor converts MQTT heartbeats into a JSON repreesntation.  

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **MQTT Controller Service** | | | The MQTT Controller service |
| Listening topic | | | The topic on which we will listen to get MQTT C2 messages |


## ConvertJSONAck

This Processor parses C2 respones (acknowledgements) and forwards them to the MQTT agent.    

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **MQTT Controller Service** | | | The MQTT Controller service |
| Listening topic | | | The topic on which we will listen to get MQTT C2 messages |

### Relationships

| Name | Description |
| - | - |
| success | Any successful http response flow file will be sent to this relationship |

## ConvertUpdate

This converts MQTT update messages into an HTTP request to retrieve an update. This
processor requires cURL support. If it does not exist this processor will be a NOOP.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **MQTT Controller Service** | | | The MQTT Controller service |
| SSL Context Service | | | SSL context service used for HTTP requestor.  |
| Listening topic | | | The topic on which we will listen to get MQTT C2 messages |

## ExecuteProcess

### Description

Runs an operating system command specified by the user and writes the output of
that command to a FlowFile. If the command is expected to be long-running, the
Processor can output the partial data on a specified interval. When this option
is used, the output is expected to be in textual format, as it typically does
not make sense to split binary data on arbitrary time-based intervals.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Command** | | | Specifies the command to be executed; if just the name of an executable is provided, it must be in the user's environment PATH.<br>**Supports Expression Language: true**  |
| Command Arguments | | | The arguments to supply to the executable delimited by white space. White space can be escaped by enclosing it in double-quotes.<br>**Supports Expression Language: true**  |
| Working Directory | | | The directory to use as the current working directory when executing the command<br>**Supports Expression Language: true**  |
| **Batch Duration** | 0  || If the process is expected to be long-running and produce textual output, a batch duration can be specified. |
| **Redirect Error Stream** | false | | If true will redirect any error stream output of the process to the output stream. |

### Relationships

| Name | Description |
| - | - |
| success | All created FlowFiles are routed to this relationship. |

## ExecuteScript

### Description

Executes a script given the flow file and a process session. The script is
responsible for handling the incoming flow file (transfer to SUCCESS or remove,
e.g.) as well as any flow files created by the script. If the handling is
incomplete or incorrect, the session will be rolled back.

Scripts must define an onTrigger function which accepts NiFi Context and
Property objects. For efficiency, scripts are executed once when the processor
is run, then the onTrigger method is called for each incoming flowfile. This
enables scripts to keep state if they wish, although there will be a script
context per concurrent task of the processor. In order to, e.g., compute an
arithmetic sum based on incoming flow file information, set the concurrent
tasks to 1.

Multiple variables are provided automatically to script contexts:

| Name | Purpose |
| - | - |
| `log` | the logging object used for logging debug, info, warn, and error |
| `REL_SUCCESS` | the "success" relationship |
| `REL_FAILURE` | the "failure" relationship |

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

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Script Engine** | python | python, lua | The engine to execute scripts |
| Script File | | | Path to script file to execute.  Only one of Script File or Script Body may be used | 
| Script Body | | | Body of script to execute.  Only one of Script File or Script Body may be used | 
| Module Directory | | | Comma-separated list of paths to files and/or directories which contain modules required by the script | 

### Relationships

| Name | Description |
| - | - |
| success | Script successes |
| failure | Script failures |

## ExecuteSQL

### Description

Execute provided SQL query. Query result rows will be outputted as new flow
files with attribute keys equal to result column names and values equal to
result values. There will be one output FlowFile per result row. This processor
can be scheduled to run using the standard timer-based scheduling methods, or
it can be triggered by an incoming FlowFile. If it is triggered by an incoming
FlowFile, then attributes of that FlowFile will be available when evaluating
the query.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Connection URL** | | | The database connection URL (e.g. `sqlite://filename.db?cache=shared`) **Only SQLite is currently supported** |
| SQL Statement | | | The SQL query to execute. The query can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL query, to be issued by the processor to the database. Note that Expression Language is not evaluated for flow file contents.<br>**Supports Expression Language: true** |

### Relationships

| Name | Description |
| - | - |
| original | Upon successful query execution, the original FlowFile is routed here. |
| success | For each SQL result row, a FlowFile will be written to this relationships. Attributes will be written to result FlowFiles having the same names and values as present in result columns. |
| failure | A FlowFile is routed to this relationship if the SQL statement cannot be executed and retrying the operation will also fail, such as an invalid query or an integrity constraint violation. |

### Reads Attributes
| Name | Description |
| - | - |
| sql.args.N.value | Incoming FlowFiles are expected to be parametrized SQL statements. The value of the Parameters are specified as `sql.args.1.value`, `sql.args.2.value`, `sql.args.3.value`, and so on. The type of the `sql.args.1.value` Parameter is specified by the `sql.args.1.type` attribute. |

## GetFile

### Description

Creates FlowFiles from files in a directory. NiFi will ignore files it doesn't
have at least read permissions for.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Batch Size** | 10 | | The maximum number of files to pull in each iteration |
| **Input Directory** | . | | The input directory from which to pull files<br>**Supports Expression Language: true** |
| **Ignore Hidden Files** | true | | Indicates whether or not hidden files should be ignored |
| **Keep Source File** | false | | If true, the file is not deleted after it has been copied to the Content Repository |
| Maximum File Age | 0 sec | | The minimum age that a file must be in order to be pulled any file younger than this amount of time (according to last modification date) will be ignored |
| **Minimum File Age** | 0 sec | | The maximum age that a file must be in order to be pulled; any file older than this amount of time (according to last modification date) will be ignored |
| Maximum File Size | 0 B | | The maximum size that a file can be in order to be pulled |
| **Minimum File Size** | 0 B | | The minimum size that a file must be in order to be pulled |
| **Polling Interval** | 0 sec | | Indicates how long to wait before performing a directory listing |
| **Recurse Subdirectories** | true | | Indicates whether or not to pull files from subdirectories |
| **File Filter** | [^\\\\.].\* | | Only files whose names match the given regular expression will be picked up |

### Relationships

| Name | Description |
| - | - |
| success | All files are routed to success |

## GetUSBCamera

### Description

Gets images from USB Video Class (UVC)-compatible devices. Outputs one flow
file per frame at the rate specified by the `FPS` property in the format
specified by the `Format` property.

Camera frames are captured in a separate background thread and are emitted as
flow files upon capture. The onTrigger of this processor is a NOOP and will
report an error if inputs are flowed into the processor. Because of this, the
standard event/timer driven scheduling options have no effect.

If the Width/Height properties are set, the closest supported image frame
dimensions to the given Width/Height properties are used.

If no Width/Height properties are set, and the camera supports multiple image
size/quality settings, the highest quality is chosen for the given FPS. For
example:

- If the FPS is 10 and the camera supports a maximum of 1920x1080 at this FPS,
  output images will be 1920x780
- If the FPS is 60 and the camera supports a maximum of 320x240 at this FPS,
  output images will be 320x240

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **FPS** | 1 | | Frames per second to capture from USB camera |
| Width | | | Desired frame width (closest supported by camera hardware will be used) |
| Height | | | Desired frame height (closest supported by camera hardware will be used) |
| **Format** | PNG | PNG, RAW | Frame format (currently only PNG and RAW are supported; RAW is a binary pixel buffer of RGB values) |
| USB Vendor ID | | | USB Vendor ID of camera device, in hexadecimal format |
| USB Product ID | | | USB Product ID of camera device, in hexadecimal format|
| USB Serial No. | | | USB Serial No. of camera device |

### Relationships

| Name | Description |
| - | - |
| success | Sucessfully captured images sent here |
| failure | Failures sent here |

## GenerateFlowFile

### Description

This processor creates FlowFiles with random data or custom content.
GenerateFlowFile is useful for load testing, configuration, and simulation.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **File Size** | 1 kB | | The size of the file that will be used |
| **Batch Size** | 1 | | The number of FlowFiles to be transferred in each invocation |
| **Data Format** | Binary | Binary, Text | Specifies whether the data should be Text or Binary|
| **Unique FlowFiles** | true | true, false | If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput |

### Relationships

| Name | Description |
| - | - |
| success | Generated FlowFiles are sent here |

## InvokeHTTP

### Description

An HTTP client processor which can interact with a configurable HTTP Endpoint.
The destination URL and HTTP Method are configurable. FlowFile attributes are
converted to HTTP headers and the FlowFile contents are included as the body of
the request (if the HTTP Method is PUT, POST or PATCH).

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **HTTP Method** | GET | | HTTP request method (GET, POST, PUT, PATCH, DELETE, HEAD, OPTIONS). Arbitrary methods are also supported. Methods other than POST, PUT and PATCH will be sent without a message body. |
| **Remote URL** | | | Remote URL which will be connected to, including scheme, host, port, path.<br>**Supports Expression Language: true** |
| **Connection Timeout** | 5 secs | | Max wait time for connection to remote service. |
| **Read Timeout** | 15 secs | | Max wait time for response from remote service. |
| **Include Date Header** | True | True, False | Include an RFC-2616 Date header in the request. |
| **Follow Redirects** | True | | Follow HTTP redirects issued by remote server. |
| Attributes to Send | | |  Regular expression that defines which attributes to send as HTTP headers in the request. If not defined, no attributes are sent as headers. |
| SSL Context Service | | | The SSL Context Service used to provide client certificate information for TLS/SSL (https) connections. |
| Proxy Host | | | The fully qualified hostname or IP address of the proxy server |
| Proxy Port | | | The port of the proxy server |
| invokehttp-proxy-user | | | Username to set when authenticating against proxy |
| invokehttp-proxy-password | | |   Password to set when authenticating against proxy |
| **Content-type** | application/octet-stream | | The Content-Type to specify for when content is being transmitted through a PUT, POST or PATCH. In the case of an empty value after evaluating an expression language expression, Content-Type defaults to application/octet-stream|
| send-message-body | true | true, false | If true, sends the HTTP message body on POST/PUT/PATCH requests (default). If false, suppresses the message body and content-type header for these requests. |
| **Use Chunked Encoding** | false | true, false | When POST'ing, PUT'ing or PATCH'ing content set this property to true in order to not pass the 'Content-length' header and instead send 'Transfer-Encoding' with a value of 'chunked'. This will enable the data transfer mechanism which was introduced in HTTP 1.1 to pass data of unknown lengths in chunks. |
| Put Response Body in Attribute | | | If set, the response body received back will be put into an attribute of the original FlowFile instead of a separate FlowFile. The attribute key to put to is determined by evaluating value of this property. |
| Always Output Response | false | true, false | Will force a response FlowFile to be generated and routed to the 'Response' relationship regardless of what the server status code received is or if the processor is configured to put the server response body in the request attribute. In the later configuration a request FlowFile with the response body in the attribute and a typical response FlowFile will be emitted to their respective relationships. |
| Penalize on "No Retry" | false | true, false | Enabling this property will penalize FlowFiles that are routed to the "No Retry" relationship. |

### Relationships

| Name | Description |
| - | - |
| success | All files are routed to success |
| response | A Response FlowFile will be routed upon success (2xx status codes). If the 'Output Response Regardless' property is true then the response will be sent to this relationship regardless of the status code received. |
| retry | The original FlowFile will be routed on any status code that can be retried (5xx status codes). It will have new attributes detailing the request. |
| no retry | The original FlowFile will be routed on any status code that should NOT be retried (1xx, 3xx, 4xx status codes). It will have new attributes detailing the request. |
| failure | The original FlowFile will be routed on any type of connection failure, timeout or general exception. It will have new attributes detailing the request. |

## LogAttribute

### Description

Logs attributes of flow files in the MiNiFi application log.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Log Level** | info | trace, debug, info, warn, error | The Log Level to use when logging the Attributes |
| Attributes to Log |  | |  A comma-separated list of Attributes to Log. If not specified, all attributes will be logged. |
| Attributes to Ignore |  | | A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored. |
| **Log Payload** | false | true, false | If true, the FlowFile's payload will be logged, in addition to its attributes; otherwise, just the Attributes will be logged. |
| Log prefix | | | Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors. |

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this relationship |

## ListenHTTP

### Description

Starts an HTTP Server and listens on a given base path to transform incoming
requests into FlowFiles. The default URI of the Service will be
`http://{hostname}:{port}/contentListener`. Only HEAD, POST, and GET requests are
supported. PUT, and DELETE will result in an error and the HTTP response
status code 405.

The response body text for all requests, by default, is empty (length of 0). A
static response body can be set for a given URI by sending input files to
ListenHTTP with the `http.type` attribute set to `response_body`. The response
body FlowFile `filename` attribute is appended to the `Base Path` property
(separated by a `/`) when mapped to incoming requests. The `mime.type`
attribute of the response body FlowFile is used for the `Content-type` header
in responses. Response body content can be cleared by sending an empty (size 0)
FlowFile for a given URI mapping.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Base Path** | contentListener | | Base path for incoming connections |
| **Listening Port** | | | The Port to listen on for incoming connections |
| SSL Certificate | | | File containing PEM-formatted file including TLS/SSL certificate and key |
| SSL Certificate Authority | | | File containing trusted PEM-formatted certificates |
| SSL Verify Peer | no | yes, no | Whether or not to verify the client's certificate  |
| SSL Minimum Version | SSL2 | SSL2, SSL3, TLS1.0, TLS1.1, TLS1.2 | Minimum TLS/SSL version allowed |
| HTTP Headers to receive as Attributes (Regex) | | | Specifies the Regular Expression that determines the names of HTTP Headers that should be passed along as FlowFile attributes |

### Relationships

| Name | Description |
| - | - |
| success | Relationship for successfully received FlowFiles |

## ListenSyslog

### Description

Listens for Syslog messages being sent to a given port over TCP or UDP.
Incoming messages are checked against regular expressions for RFC5424 and
RFC3164 formatted messages. The format of each message is:
(\<PRIORITY\>)(VERSION )(TIMESTAMP) (HOSTNAME) (BODY) where version is
optional. The timestamp can be an RFC5424 timestamp with a format of
"yyyy-MM-dd'T'HH:mm:ss.SZ" or "yyyy-MM-dd'T'HH:mm:ss.S+hh:mm", or it can be an
RFC3164 timestamp with a format of "MMM d HH:mm:ss". If an incoming messages
matches one of these patterns, the message will be parsed and the individual
pieces will be placed in FlowFile attributes, with the original message in the
content of the FlowFile. If an incoming message does not match one of these
patterns it will not be parsed and the syslog.valid attribute will be set to
false with the original message in the content of the FlowFile. Valid messages
will be transferred on the success relationship, and invalid messages will be
transferred on the invalid relationship.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Receive Buffer Size** | 65507 B | | The size of each buffer used to receive Syslog messages. Adjust this value appropriately based on the expected size of the incoming Syslog messages. When UDP is selected each buffer will hold one Syslog message. When TCP is selected messages are read from an incoming connection until the buffer is full, or the connection is closed. |
| **Max Size of Socket Buffer** | 1 MB | | The maximum size of the socket buffer that should be used. This is a suggestion to the Operating System to indicate how big the socket buffer should be. If this value is set too low, the buffer may fill up before the data can be read, and incoming data will be dropped. |
| **Max Number of TCP Connections** | 2 | | The maximum number of concurrent connections to accept Syslog messages in TCP mode. |
| **Max Batch Size** | 1 | | The maximum number of Syslog events to add to a single FlowFile. If multiple events are available, they will be concatenated along with the \<Message Delimiter\> up to this configured maximum number of messages|
| **Message Delimiter"** | \n | | Specifies the delimiter to place between Syslog messages when multiple messages are bundled together (see \<Max Batch Size\> property). |
| **Parse Messages** | true | true, false | Indicates if the processor should parse the Syslog messages. If set to false, each outgoing FlowFile will only contain the sender, protocol, and port, and no additional attributes. |
| **Port** | 514 | | The port for Syslog communication.|

### Relationships

| Name | Description |
| - | - |
| success | Syslog messages that match one of the expected formats will be sent out this relationship as a FlowFile per message. |
| invalid | Syslog messages that do not match one of the expected formats will be sent out this relationship as a FlowFile per message. |

## PutFile

### Description

Writes the contents of a FlowFile to the local file system

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Directory** | | | The directory to which files should be written. You may use expression language such as `/aa/bb/${path}`<br>**Supports Expression Language: true** |
| **Conflict Resolution Strategy** | fail | replace, ignore, fail | Indicates what should happen when a file with the same name already exists in the output directory |
| **Create Missing Directories** | true | true, false | If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure. |
| **Maximum File Count** | | | Specifies the maximum number of files that can exist in the output directory |

### Relationships

| Name | Description |
| - | - |
| success | Files that have been successfully written to the output directory are transferred to this relationship |
| failure | Files that could not be written to the output directory for some reason are transferred to this relationship |

## PutSQL

### Description

Executes a SQL UPDATE or INSERT command. The content of an incoming FlowFile is
expected to be the SQL command to execute. The SQL command may use the `?`
character to bind parameters. In this case, the parameters to use must exist as
FlowFile attributes with the naming convention `sql.args.N.type` and
`sql.args.N.value`, where `N` is a positive integer. The content of the
FlowFile is expected to be in UTF-8 format.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Connection URL** | | | The database connection URL (e.g. `sqlite://filename.db?cache=shared`) **Only SQLite is currently supported** |
| SQL Statement | | | The SQL statement to execute. The statement can be empty, a constant value, or built from attributes using Expression Language. If this property is specified, it will be used regardless of the content of incoming flowfiles. If this property is empty, the content of the incoming flow file is expected to contain a valid SQL statement, to be issued by the processor to the database.<br>**Supports Expression Language: true** |
| **Batch Size** | 1 | | The maximum number of FlowFiles to put to the database in a single transaction |

### Relationships

| Name | Description |
| - | - |
| retry | A FlowFile is routed to this relationship if the database cannot be updated but attempting the operation again may succeed |
| success | A FlowFile is routed to this relationship after the database is successfully updated |
| failure | A FlowFile is routed to this relationship if the database cannot be updated and retrying the operation will also fail, such as an invalid query or an integrity constraint violation |

### Reads Attributes
| Name | Description |
| - | - |
| sql.args.N.value | Incoming FlowFiles are expected to be parametrized SQL statements. The value of the Parameters are specified as `sql.args.1.value`, `sql.args.2.value`, `sql.args.3.value`, and so on. The type of the `sql.args.1.value` Parameter is specified by the `sql.args.1.type` attribute. |

## RouteOnAttribute

### Description

Routes FlowFiles based on their Attributes using the Attribute Expression Language.

### Dynamic Properties

Dynamic Properties allow the user to specify both the name and value of a property.

| Name | Value | Description |
| - | - | - |
| Relationship Name | Attribute Expression Language | Routes FlowFiles whose attributes match the Attribute Expression Language specified in the Dynamic Property Value to the Relationship specified in the Dynamic Property Key<br>**Supports Expression Language: true** |

### Relationships

| Name | Description |
| - | - |
| unmatched | FlowFiles that do not match any user-define expression will be routed here |

### Dynamic Relationships

| Name | Description |
| - | - |
| Name from Dynamic Property | FlowFiles that match the Dynamic Property's Attribute Expression Language |

## TailFile

### Description

"Tails" a file, or a list of files, ingesting data from the file as it is
written to the file. The file is expected to be textual. Data is ingested only
when a new line is encountered (carriage return or new-line character or
combination). If the file to tail is periodically "rolled over", as is
generally the case with log files, an optional Rolling Filename Pattern can be
used to retrieve data from files that have rolled over, even if the rollover
occurred while NiFi was not running (provided that the data still exists upon
restart of NiFi). It is generally advisable to set the Run Schedule to a few
seconds, rather than running with the default value of 0 secs, as this
Processor will consume a lot of resources if scheduled very aggressively. At
this time, this Processor does not support ingesting files that have been
compressed when 'rolled over'.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **File to Tail** | | | Fully-qualified filename of the file that should be tailed |
| **State File** | TailFileState | | Specifies the file that should be used for storing state about what data has been ingested so that upon restart NiFi can resume from where it left off |
| Input Delimiter | | | Specifies the character that should be used for delimiting the data being tailed from the incoming file. |

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this Relationship. |

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

## MergeContent

Merges a Group of FlowFiles together based on a user-defined strategy and
packages them into a single FlowFile. It is recommended that the Processor be
configured with only a single incoming connection, as Group of FlowFiles will
not be created from FlowFiles in different connections. This processor updates
the mime.type attribute as appropriate.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Merge Strategy** | Defragment | Defragment, Bin-Packing Algorithm | |
| **Merge Format** | Binary Concatenation | TAR, ZIP, "FlowFile Stream, v3", "FlowFile Stream, v2", "FlowFile Tar, v1", Binary Concatenation, Avro | Determines the format that will be used to merge the content. |
| Correlation Attribute Name | | | Correlation Attribute Name |
| **Delimiter Strategy** | Filename | Filename, Text | Determines if Header, Footer, and Demarcator should point to files containing the respective content, or if the values of the properties should be used as the content. |
| Header File | | | Filename specifying the header to use |
| Footer File | | | Filename specifying the footer to use |
| Demarcator File | | | Filename specifying the demarcator to use |
| **Keep Path** | false | false, true | If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry |

### Relationships

| Name | Description |
| - | - |
| original | The FlowFiles that were used to create the bundle |
| merged | The FlowFile containing the merged content |
| failure | If the bundle cannot be created, all FlowFiles that would have been used to created the bundle will be transferred to failure |

## ExtractText

Extracts the content of a FlowFile and places it into an attribute.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Attribute** | | | Attribute to set from content |
| **Size Limit** | 2 MB | | Maximum number of bytes to read into the attribute. 0 for no limit. |

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this Relationship. |

## CompressContent

Compresses or decompresses the contents of FlowFiles using a user-specified
compression algorithm and updates the mime.type attribute as appropriate

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Compression Level** | 1 | 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 | The compression level to use; this is valid only when using GZIP compression. |
| **Mode** | compress | compress, decompress | Indicates whether the processor should compress content or decompress content. |
| **Compression Format** | use mime.type attribute | gzip, bzip2, xz-lzma2, lzma, snappy, snappy framed | The compression format to use. |
| **Update Filename** | false | true, false | If true, will remove the filename extension when decompressing data (only if the extension indicates the appropriate compression format) and add the appropriate extension when compressing data |

### Relationships

| Name | Description |
| - | - |
| success | FlowFiles will be transferred to the success relationship after successfully being compressed or decompressed |
| failure | FlowFiles will be transferred to the failure relationship if they fail to compress/decompress |

## FocusArchiveEntry

Allows manipulation of entries within an archive (e.g. TAR) by focusing on one
entry within the archive at a time. When an archive entry is focused, that
entry is treated as the content of the FlowFile and may be manipulated
independently of the rest of the archive. To restore the FlowFile to its
original state, use UnfocusArchiveEntry.

Archives may be compressed.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Path** |  | | The path of the archive entry to focus |

### Relationships

| Name | Description |
| - | - |
| success | The FlowFile, with the given entry focused, is sent to this relationship |

## UnfocusArchiveEntry

Restores a FlowFile which has had an archive entry focused via
FocusArchiveEntry to its original state.

### Relationships

| Name | Description |
| - | - |
| success | All FlowFiles are routed to this Relationship |

## ManipulateArchive

Performs an operation which manipulates an archive without needing to split the
archive into multiple FlowFiles.

### Operations

| Name | Description |
| - | - |
| `remove` | Removes the target |
| `copy` | Copies the target to a new archive entry |
| `move` | Moves the target to a new archive entry |
| `touch` | Creates a new archive entry |

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Operation** | | touch, remove, copy, move | Operation to perform on the archive |
| Target | | | An existing entry within the archive to perform the operation on |
| Destination | | | Destination for operations (touch, move or copy) which result in new entries. |
| Before | | | For operations which result in new entries, places the new entry before the entry specified by this property |
| After | | | For operations which result in new entries, places the new entry after the entry specified by this property |

### Relationships

| Name | Description |
| - | - |
| success | FlowFiles will be transferred to the success relationship if the operation succeeds. |
| failure | FlowFiles will be transferred to the failure relationship if the operation fails. |

## PublishKafka 

This Processor puts the contents of a FlowFile to a Topic in Apache Kafka. The
content of a FlowFile becomes the contents of a Kafka message. This message is
optionally assigned a key by using the \<Kafka Key\> Property.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Known Brokers** | | | A comma-separated list of known Kafka Brokers in the format \<host\>:\<port\> |
| **Topic Name** | | | The Kafka Topic of interest |
| **Delivery Guarantee** | 1 | all, 1, 0 | Specifies the requirement for guaranteeing that a message is sent to Kafka |
| Request Timeout | | | The ack timeout of the producer request in milliseconds |
| Client Name | | | Client Name to use when communicating with Kafka |
| Batch Size | | | Maximum number of messages batched in one MessageSet |
| Attributes to Send as Headers | | | Any attribute whose name matches the regex will be added to the Kafka messages as a Header |
| Queue Buffering Max Time | | | Delay to wait for messages in the producer queue to accumulate before constructing message batches |
| Queue Max Buffer Size | | | Maximum total message size sum allowed on the producer queue |
| Queue Max Message | | | Maximum number of messages allowed on the producer queue |
| Compress Codec | none | none, gzip, snappy | compression codec to use for compressing message sets |
| Max Flow Segment Size | | | Maximum flow content payload segment size for the kafka record |
| Security Protocol | | plaintext, ssl, sasl\_plaintext, sasl\_ssl | Protocol used to communicate with brokers |
| Security Cert | | | Path to client's public key (PEM) used for authentication |
| Security Private Key | | | Path to client's private key (PEM) used for authentication |
| Security Pass Phrase | | | Private key passphrase |

### Relationships

| Name | Description |
| - | - |
| success | Any FlowFile that is successfully sent to Kafka will be routed to this Relationship |
| failure | Any FlowFile that cannot be sent to Kafka will be routed to this Relationship |

## PublishMQTT

This Processor puts the contents of a FlowFile to a MQTT broker for a sepcified topic. The
content of a FlowFile becomes the payload of the MQTT message.

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Broker URI** | | | The URI to use to connect to the MQTT broker |
| **Topic** | | | The topic to publish the message to |
| Session state | | | Whether to start afresh or resume previous flows |
| Client ID | | | MQTT client ID to use |
| Username | | | Username to use when connecting to the broker |
| Password | | | Password to use when connecting to the broker |
| Keep Alive Interval | | | Defines the maximum time interval between messages sent or received |
| Connection Timeout | | | Maximum time interval the client will wait for the network connection to the MQTT server |
| Quality of Service | | | The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2' |
| Retain | | | Retain MQTT published record in broker |
| Max Flow Segment Size | | Maximum flow content payload segment size for the MQTT record |

### Relationships

| Name | Description |
| - | - |
| success | Any FlowFile that is successfully sent to broker will be routed to this Relationship |
| failure | Any FlowFile that cannot be sent to broker will be routed to this Relationship |

## ConsumeMQTT

This Processor gets the contents of a FlowFile from a MQTT broker for a sepcified topic. The
the payload of the MQTT message becomes content of a FlowFile

### Properties

In the list below, the names of required properties appear in bold. Any other
properties (not in bold) are considered optional. The table also indicates any
default values, and whether a property supports the NiFi Expression Language.

| Name | Default Value | Allowable Values | Description |
| - | - | - | - |
| **Broker URI** | | | The URI to use to connect to the MQTT broker |
| **Topic** | | | The topic to publish the message to |
| Session state | | | Whether to start afresh or resume previous flows |
| Client ID | | | MQTT client ID to use |
| Username | | | Username to use when connecting to the broker |
| Password | | | Password to use when connecting to the broker |
| Keep Alive Interval | | | Defines the maximum time interval between messages sent or received |
| Connection Timeout | | | Maximum time interval the client will wait for the network connection to the MQTT server |
| Quality of Service | | | The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2' |
| Max Flow Segment Size | | Maximum flow content payload segment size for the MQTT record |

### Relationships

| Name | Description |
| - | - |
| success | Any FlowFile that is successfully sent to broker will be routed to this Relationship |

## UpdateAttribute

This processor updates the attributes of a FlowFile using properties that are
added by the user. This allows you to set default attribute changes that affect
every FlowFile going through the processor, equivalent to the "basic" usage in
Apache NiFi.

### Properties

The properties in this processor are added by the user. The expression language
is supported in user-added properties for this processor. See the [NiFi
Expression Language Guide](EXPRESSIONS.md) to learn how to formulate proper
expression language statements to perform the desired functions.

### Relationships

| Name | Description |
| - | - |
| success | If the processor successfully updates the specified attribute(s), then the FlowFile follows this relationship. |

### Basic Usage

For basic usage, changes are made by adding a new processor property and
referencing as its name the attribute you want to change. Then enter the
desired attribute value as the Value. The Value can be as simple as any text
string or it can be a NiFi Expression Language statement that specifies how to
formulate the value. (See the [NiFi Expression Language Usage
Guide](EXPRESSIONS.md) for details on crafting NiFi Expression Language
statements.)

As an example, to alter the standard `filename` attribute so that it has `.txt`
appended to the end of it, add a new property and make the property name
`filename` (to reference the desired attribute), and as the value, use the NiFi
Expression Language statement shown below:

- **Property**: `filename`
- **Value**: `${filename}.txt`

The preceding example illustrates how to modify an existing attribute. If an
attribute does not already exist, this processor can also be used to add a new
attribute. For example, the following property could be added to create a new
attribute called `myAttribute` that has the value `myValue`:

- **Property**: `myAttribute`
- **Value**: `myValue`

In this example, all FlowFiles passing through this processor will receive an
additional FlowFile attribute called `myAttribute` with the value `myValue`. This
type of configuration might be used in a flow where you want to tag every
FlowFile with an attribute so that it can be used later in the flow, such as
for routing in a `RouteOnAttribute` processor.

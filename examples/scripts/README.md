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
# ExecuteScript Examples

The following examples show how scripts can be integrated into MiNiFi using the [ExecuteScript](../../PROCESSORS.md#executescript) processor.

## Heads or Tails
This script will generate empty flowfiles and transfer them randomly to the Success or Failure relationship of the ExecuteScript.
- [heads_or_tails.lua](lua/heads_or_tails.lua)
- [heads_or_tails.py](python/heads_or_tails.py)

## Reverse FlowFile's content
This script reverses the content of the incoming flowfiles, and adds the current timestamp as an attribute.  
- [reverse_flow_file_content.lua](lua/reverse_flow_file_content.lua)
- [reverse_flow_file_content.py](python/reverse_flow_file_content.py)



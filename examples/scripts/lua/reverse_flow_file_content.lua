---
---  Licensed to the Apache Software Foundation (ASF) under one or more
---  contributor license agreements.  See the NOTICE file distributed with
---  this work for additional information regarding copyright ownership.
---  The ASF licenses this file to You under the Apache License, Version 2.0
---  (the "License"); you may not use this file except in compliance with
---  the License.  You may obtain a copy of the License at
---
---      http://www.apache.org/licenses/LICENSE-2.0
---
---  Unless required by applicable law or agreed to in writing, software
---  distributed under the License is distributed on an "AS IS" BASIS,
---  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
---  See the License for the specific language governing permissions and
---  limitations under the License.
---

read_callback = {}
write_reverse_string_callback = {}

function read_callback.process(self, input_stream)
    self['content'] = input_stream:read(0)
    return #self['content']
end

function write_reverse_string_callback.process(self, output_stream)
    reversed_content = string.reverse(self['content'])
    output_stream:write(reversed_content)
    return #reversed_content
end

function onTrigger(context, session)
    flow_file = session:get()

    if flow_file ~= nil then
        session:read(flow_file, read_callback)
        write_reverse_string_callback['content'] = read_callback['content']
        session:write(flow_file, write_reverse_string_callback)
        flow_file:addAttribute('lua_timestamp', tostring(os.time()))
        session:transfer(flow_file, REL_SUCCESS)
    end
end

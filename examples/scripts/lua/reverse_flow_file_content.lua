--- SPDX-License-Identifier: Apache-2.0
--- SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
--- SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

--- SPDX-License-Identifier: Apache-2.0
--- SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
--- SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
local foo_module = require "foo"
local bar_module = require "bar"


function onTrigger(context, session)
    flow_file = session:get()
    log:info(foo_module.foo() .. bar_module.bar())
    if flow_file ~= nil then
        session:transfer(flow_file, REL_SUCCESS)
    end
end

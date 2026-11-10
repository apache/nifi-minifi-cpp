--- SPDX-License-Identifier: Apache-2.0
--- SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
--- SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

math.randomseed(os.time())

function onTrigger(context, session)
    flow_file = session:create()

    if flow_file ~= nil then
        if math.random() > 0.5 then
            session:transfer(flow_file, REL_SUCCESS)
        else
            session:transfer(flow_file, REL_FAILURE)
        end
    end
end

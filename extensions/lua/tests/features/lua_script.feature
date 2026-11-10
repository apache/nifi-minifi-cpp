# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

@ENABLE_LUA_SCRIPTING
Feature: MiNiFi can execute Lua scripts

  Scenario: ExecuteScript should only allow the number of parallel tasks defined by the max concurrent tasks attribute for Lua scripts
    Given a GenerateFlowFile processor with the "File Size" property set to "0B"
    And the scheduling period of the GenerateFlowFile processor is set to "500 ms"
    And a host resource file "sleep_forever.lua" is bound to the "/tmp/resources/lua/sleep_forever.lua" path in the MiNiFi container
    And a ExecuteScript processor with the "Script File" property set to "/tmp/resources/lua/sleep_forever.lua"
    And ExecuteScript is EVENT_DRIVEN
    And the "Script Engine" property of the ExecuteScript processor is set to "lua"
    And the max concurrent tasks attribute of the ExecuteScript processor is set to 3
    And the "success" relationship of the GenerateFlowFile processor is connected to the ExecuteScript
    And ExecuteScript's success relationship is auto-terminated

    When the MiNiFi instance starts up
    Then the Minifi logs contain the following message: "Sleeping forever" 3 times after 5 seconds

<!--
  SPDX-License-Identifier: Apache-2.0
  SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
  SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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



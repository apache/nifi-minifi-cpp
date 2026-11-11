---  SPDX-License-Identifier: Apache-2.0
---  SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
---  SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

function sleep(n)
  local t0 = os.clock()
  while os.clock() - t0 <= n do end
end

function onTrigger(context, session)
  log:info('Sleeping forever')

  while true do
    sleep(1)
  end
end

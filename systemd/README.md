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
# Apache NiFi - Installing MiNiFi-C++ as a systemd service

1. Extract the compiled binary package under /opt. You should get a subdirectory named 'nifi-minifi-cpp-<version>'. Rename it to nifi-minifi-cpp for easier management.
    ```
    cd /opt
    tar xvzf /path/to/nifi-minifi-cpp-*-bin-linux.tar.gz
    mv nifi-minifi-cpp-* nifi-minifi-cpp
    ```
2. Copy minifi-cpp.service to /lib/systemd/system or /usr/lib/systemd/system, depending on where your distribution stores systemd service units.
3. Copy envfile to /opt/nifi-minifi-cpp
4. Reload systemd units
    ```
    systemctl daemon-reload
    ```
5. [Configure](../CONFIGURE.md) the agent
6. Enable the agent if you want to start it on system startup, and start the agent.
   ```
   systemctl enable minifi-cpp.service
   systemctl start minifi-cpp.service
   ```


# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

class ModbusChecker:
    def __init__(self, container_communicator):
        self.container_communicator = container_communicator

    def set_value_on_plc_with_modbus(self, container_name, modbus_cmd):
        print(modbus_cmd)
        (code, output) = self.container_communicator.execute_command(container_name, ["modbus", "localhost", modbus_cmd])
        print(output)
        return code == 0

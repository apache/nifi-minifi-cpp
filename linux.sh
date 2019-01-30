# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#!/bin/bash
verify_gcc_enable(){
  feature="$1"
  feature_status=${!1}
  if [ "$feature" = "BUSTACHE_ENABLED" ]; then 
    if (( COMPILER_MAJOR == 6 && COMPILER_MINOR >= 3 && COMPILER_REVISION >= 1  )); then
      echo "true" 	    
    elif (( COMPILER_MAJOR > 6 )); then
      echo "true"
    else
      echo "false"
    fi
  elif [ "$feature" = "EXECUTE_SCRIPT_ENABLED" ]; then
    if (( COMPILER_MAJOR >= 6 )); then
      echo "true"
    else
      echo "false"
    fi
  elif [ "$feature" = "KAFKA_ENABLED" ]; then
    if (( COMPILER_MAJOR >= 4 )); then
      if (( COMPILER_MAJOR > 4 || COMPILER_MINOR >= 8 )); then
        echo "true"
      else
        echo "false"
      fi
    else
      echo "false"
    fi
   elif [ "$feature" = "PCAP_ENABLED" ]; then
    if (( COMPILER_MAJOR >= 4 )); then
      if (( COMPILER_MAJOR > 4 || COMPILER_MINOR >= 8 )); then
        echo "true"
      else
        echo "false"
      fi
    else
      echo "false"
    fi
  else
    echo "true"
  fi
}

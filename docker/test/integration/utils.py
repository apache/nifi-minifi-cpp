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


import time
import functools
import os
import subprocess
from typing import Optional


def retry_check(max_tries=5, retry_interval=1):
    def retry_check_func(func):
        @functools.wraps(func)
        def retry_wrapper(*args, **kwargs):
            for i in range(max_tries):
                if func(*args, **kwargs):
                    return True
                time.sleep(retry_interval)
            return False
        return retry_wrapper
    return retry_check_func


def decode_escaped_str(str):
    special = {"n": "\n", "v": "\v", "t": "\t", "f": "\f", "r": "\r", "a": "\a", "\\": "\\"}
    escaped = False
    result = ""
    for ch in str:
        if escaped:
            if ch in special:
                result += special[ch]
            else:
                result += "\\" + ch
            escaped = False
        elif ch == "\\":
            escaped = True
        else:
            result += ch
    if escaped:
        result += "\\"
    return result


def is_temporary_output_file(filepath):
    return filepath.split(os.path.sep)[-1][0] == '.'


def get_minifi_pid() -> int:
    return int(subprocess.run(["pidof", "-s", "minifi"], capture_output=True).stdout)


def get_peak_memory_usage(pid: int) -> Optional[int]:
    with open("/proc/" + str(pid) + "/status") as stat_file:
        for line in stat_file:
            if "VmHWM" in line:
                peak_resident_set_size = [int(s) for s in line.split() if s.isdigit()].pop()
                return peak_resident_set_size * 1024
    return None


def get_memory_usage(pid: int) -> Optional[int]:
    with open("/proc/" + str(pid) + "/status") as stat_file:
        for line in stat_file:
            if "VmRSS" in line:
                resident_set_size = [int(s) for s in line.split() if s.isdigit()].pop()
                return resident_set_size * 1024
    return None


def wait_for(action, timeout_seconds, check_period=1, *args, **kwargs):
    start_time = time.perf_counter()
    while True:
        result = action(*args, **kwargs)
        if result:
            return result
        time.sleep(check_period)
        if timeout_seconds < (time.perf_counter() - start_time):
            break
    return False

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


def retry_check(max_tries=5, retry_interval=1):
    def retry_check_func(func):
        @functools.wraps(func)
        def retry_wrapper(*args, **kwargs):
            current_retry_count = 0
            while current_retry_count < max_tries:
                if not func(*args, **kwargs):
                    current_retry_count += 1
                    time.sleep(retry_interval)
                else:
                    return True
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

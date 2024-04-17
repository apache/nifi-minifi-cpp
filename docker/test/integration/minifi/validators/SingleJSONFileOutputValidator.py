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


import logging
import os
import json

from .FileOutputValidator import FileOutputValidator
from utils import is_temporary_output_file


class SingleJSONFileOutputValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_content):
        self.expected_content = json.loads(expected_content)

    def file_matches_json_content(self, dir_path, expected_json_content):
        listing = os.listdir(dir_path)
        if not listing:
            return 0
        for file_name in listing:
            full_path = os.path.join(dir_path, file_name)
            if not os.path.isfile(full_path) or is_temporary_output_file(full_path):
                continue
            with open(full_path, 'r') as out_file:
                file_json_content = json.loads(out_file.read())
                if file_json_content != expected_json_content:
                    print(f"JSON doesnt match actual: {file_json_content}, expected: {expected_json_content}")
                return file_json_content == expected_json_content
        return False

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        return self.get_num_files(full_dir) == 1 and self.file_matches_json_content(full_dir, self.expected_content)

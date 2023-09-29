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
import re

from os import listdir
from os.path import join
from utils import is_temporary_output_file

from .OutputValidator import OutputValidator


class FileOutputValidator(OutputValidator):
    def set_output_dir(self, output_dir):
        self.output_dir = output_dir

    @staticmethod
    def num_files_matching_regex_in_dir(dir_path: str, expected_content_regex: str):
        listing = listdir(dir_path)
        if not listing:
            return 0
        files_of_matching_content_found = 0
        for file_name in listing:
            full_path = join(dir_path, file_name)
            if not os.path.isfile(full_path) or is_temporary_output_file(full_path):
                continue
            with open(full_path, 'r') as out_file:
                content = out_file.read()
                if re.search(expected_content_regex, content):
                    files_of_matching_content_found += 1
        return files_of_matching_content_found

    @staticmethod
    def num_files_matching_content_in_dir(dir_path, expected_content):
        listing = listdir(dir_path)
        if not listing:
            return 0
        files_of_matching_content_found = 0
        for file_name in listing:
            full_path = join(dir_path, file_name)
            if not os.path.isfile(full_path) or is_temporary_output_file(full_path):
                continue
            with open(full_path, 'r') as out_file:
                contents = out_file.read()
                logging.info("dir %s -- name %s", dir_path, file_name)
                logging.info("expected content: %s -- actual: %s, match: %r", expected_content, contents, expected_content in contents)
                if expected_content in contents:
                    files_of_matching_content_found += 1
        return files_of_matching_content_found

    @staticmethod
    def get_num_files(dir_path):
        listing = listdir(dir_path)
        logging.info("Num files in %s: %d", dir_path, len(listing))
        if not listing:
            return 0
        files_found = 0
        for file_name in listing:
            full_path = join(dir_path, file_name)
            if os.path.isfile(full_path) and not is_temporary_output_file(full_path):
                logging.info("Found output file in %s: %s", dir_path, file_name)
                files_found += 1
        return files_found

    @staticmethod
    def get_num_files_with_min_size(dir_path: str, min_size: int):
        listing = listdir(dir_path)
        logging.info("Num files in %s: %d", dir_path, len(listing))
        if not listing:
            return 0
        files_found = 0
        for file_name in listing:
            full_path = join(dir_path, file_name)
            if os.path.isfile(full_path) and not is_temporary_output_file(full_path) and os.path.getsize(full_path) >= min_size:
                logging.info("Found output file in %s: %s", dir_path, file_name)
                files_found += 1
        return files_found

    def validate(self, dir=''):
        pass

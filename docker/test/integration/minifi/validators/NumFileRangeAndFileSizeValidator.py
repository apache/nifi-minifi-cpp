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

from .FileOutputValidator import FileOutputValidator


class NumFileRangeAndFileSizeValidator(FileOutputValidator):
    def __init__(self, min_files: int, max_files: int, min_size: int):
        self.min_files = min_files
        self.max_files = max_files
        self.min_size = min_size

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        num_files = self.get_num_files_with_min_size(full_dir, self.min_size)
        logging.info("Number of files with min size %d generated: %d", self.min_size, num_files)
        return self.min_files <= num_files and num_files <= self.max_files

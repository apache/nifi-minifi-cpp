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


class SingleOrMultiFileOutputRegexValidator(FileOutputValidator):
    """
    Validates the content of a single or multiple files in the given directory.
    """

    def __init__(self, expected_content_regex: str):
        self.expected_content_regex = expected_content_regex

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        return 0 < self.num_files_matching_regex_in_dir(full_dir, self.expected_content_regex)

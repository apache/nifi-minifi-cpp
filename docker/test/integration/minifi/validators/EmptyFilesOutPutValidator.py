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

from os import listdir

from .FileOutputValidator import FileOutputValidator


class EmptyFilesOutPutValidator(FileOutputValidator):

    """
    Validates if all the files in the target directory are empty and at least one exists
    """
    def __init__(self):
        self.valid = False

    def validate(self, dir=''):

        if self.valid:
            return True

        full_dir = self.output_dir + dir
        logging.info("Output folder: %s", full_dir)
        listing = listdir(full_dir)
        if listing:
            self.valid = 0 < self.get_num_files(full_dir) and all(os.path.getsize(os.path.join(full_dir, x)) == 0 for x in listing)

        return self.valid

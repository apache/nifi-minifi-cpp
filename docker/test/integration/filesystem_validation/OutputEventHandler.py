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
import threading
import os
from utils import is_temporary_output_file

from watchdog.events import FileSystemEventHandler


class OutputEventHandler(FileSystemEventHandler):
    def __init__(self, done_event):
        self.done_event = done_event
        self.files_created_lock = threading.Lock()
        self.files_created = set()

    def get_num_files_created(self):
        with self.files_created_lock:
            logging.info("file count created: %d", len(self.files_created))
            return len(self.files_created)

    def on_created(self, event):
        if os.path.isfile(event.src_path) and not is_temporary_output_file(event.src_path):
            logging.info("Output file created: %s", event.src_path)
            with self.files_created_lock:
                self.files_created.add(event.src_path)
            self.done_event.set()

    def on_modified(self, event):
        if os.path.isfile(event.src_path) and not is_temporary_output_file(event.src_path):
            logging.info("Output file modified: %s", event.src_path)
            with self.files_created_lock:
                self.files_created.add(event.src_path)
            self.done_event.set()

    def on_moved(self, event):
        if os.path.isfile(event.dest_path):
            logging.info("Output file moved from: %s to: %s", event.src_path, event.dest_path)
            file_count_modified = False
            if event.src_path in self.files_created:
                self.files_created.remove(event.src_path)
                file_count_modified = True

            if not is_temporary_output_file(event.dest_path):
                with self.files_created_lock:
                    self.files_created.add(event.dest_path)
                file_count_modified = True

            if file_count_modified:
                self.done_event.set()

import logging
import threading
import os
import time

from watchdog.events import FileSystemEventHandler


class OutputEventHandler(FileSystemEventHandler):
    def __init__(self, done_event):
        self.done_event = done_event
        self.files_created_lock = threading.Lock()
        self.files_created = 0

    def get_num_files_created(self):
        with self.files_created_lock:
          return self.files_created

    def on_created(self, event):
        if os.path.isfile(event.src_path):
          logging.info("Output file created: " + event.src_path)
          with open(os.path.abspath(event.src_path), "r") as out_file:
            logging.info("Contents: %s", out_file.read())
          with self.files_created_lock:
            self.files_created += 1
        self.done_event.set()

    def on_modified(self, event):
        if os.path.isfile(event.src_path):
          logging.info("Output file modified: " + event.src_path)
          with open(os.path.abspath(event.src_path), "r") as out_file:
            logging.info("Contents: %s", out_file.read())

    def on_deleted(self, event):
        logging.info("Output file deleted: " + event.src_path)

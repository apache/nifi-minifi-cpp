import logging
import threading
import os
from .utils import is_temporary_output_file

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
            with open(os.path.abspath(event.src_path), "r") as out_file:
                logging.info("Contents: %s", out_file.read())
            with self.files_created_lock:
                self.files_created.add(event.src_path)
            self.done_event.set()

    def on_modified(self, event):
        if os.path.isfile(event.src_path) and not is_temporary_output_file(event.src_path):
            logging.info("Output file modified: %s", event.src_path)
            with open(os.path.abspath(event.src_path), "r") as out_file:
                logging.info("Contents: %s", out_file.read())
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
                with open(os.path.abspath(event.dest_path), "r") as out_file:
                    logging.info("Contents: %s", out_file.read())
                with self.files_created_lock:
                    self.files_created.add(event.dest_path)
                file_count_modified = True

            if file_count_modified:
                self.done_event.set()

    def on_deleted(self, event):
        logging.info("Output file deleted: " + event.src_path)

import logging

from watchdog.events import FileSystemEventHandler

class OutputEventHandler(FileSystemEventHandler):
    def __init__(self, done_event):
        self.done_event = done_event

    def on_created(self, event):
        logging.info('Output file created: ' + event.src_path)
        self.done_event.set()

    def on_modified(self, event):
        logging.info('Output file modified: ' + event.src_path)

    def on_deleted(self, event):
        logging.info('Output file modified: ' + event.src_path)

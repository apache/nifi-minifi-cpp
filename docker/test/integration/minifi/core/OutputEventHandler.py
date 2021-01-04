import logging

from watchdog.events import FileSystemEventHandler

class OutputEventHandler(FileSystemEventHandler):
    def __init__(self, validator, done_event):
        self.validator = validator
        self.done_event = done_event

    def on_created(self, event):
        logging.info('Output file created: ' + event.src_path)
        self.check(event)

    def on_modified(self, event):
        logging.info('Output file modified: ' + event.src_path)
        self.check(event)

    def check(self, event):
        if self.validator.validate():
            logging.info('Output file is valid')
            self.done_event.set()
        else:
            logging.info('Output file is invalid')


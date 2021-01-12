import logging
from threading import Event

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

from .OutputEventHandler import OutputEventHandler
from ..validators.FileOutputValidator import FileOutputValidator

class FileSystemObserver(object):
    def __init__(self, tmp_test_output_dir, output_validator):

        # TODO: extract this
        self.tmp_test_output_dir = tmp_test_output_dir

        # Point output validator to ephemeral output dir
        self.output_validator = output_validator
        if isinstance(output_validator, FileOutputValidator):
            output_validator.set_output_dir(self.tmp_test_output_dir)

        # Start observing output dir
        self.done_event = Event()
        self.event_handler = OutputEventHandler(self.output_validator, self.done_event)
        self.observer = Observer()
        self.observer.schedule(self.event_handler, self.tmp_test_output_dir)
        self.observer.start()

    def set_output_validator_subdir(self, subdir):
        self.output_validator.subdir = subdir

    def restart_observer_if_needed(self):
        if self.observer.is_alive():
            return

        self.observer = Observer()
        self.done_event.clear()
        self.observer.schedule(self.event_handler, self.tmp_test_output_dir)
        self.observer.start()

    def wait_for_output(self, timeout_seconds):
        logging.info('Waiting up to %d seconds for test output...', timeout_seconds)
        self.restart_observer_if_needed()
        self.done_event.wait(timeout_seconds)
        self.observer.stop()
        self.observer.join()

    def validate_output(self):
        return self.output_validator.validate()

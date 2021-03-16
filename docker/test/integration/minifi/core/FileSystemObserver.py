import logging
import time
from threading import Event

from watchdog.events import FileSystemEventHandler
from watchdog.observers import Observer

from .OutputEventHandler import OutputEventHandler
from ..validators.FileOutputValidator import FileOutputValidator

class FileSystemObserver(object):
    def __init__(self, test_output_dir):

        self.test_output_dir = test_output_dir

        # Start observing output dir
        self.done_event = Event()
        self.event_handler = OutputEventHandler(self.done_event)
        self.observer = Observer()
        self.observer.schedule(self.event_handler, self.test_output_dir, recursive=True)
        self.observer.start()

    def get_output_dir(self):
        return self.test_output_dir

    def restart_observer_if_needed(self):
        if self.observer.is_alive():
            return

        self.observer = Observer()
        self.done_event.clear()
        self.observer.schedule(self.event_handler, self.test_output_dir, recursive=True)
        self.observer.start()

    def wait_for_output(self, timeout_seconds, output_validator, max_files):
        logging.info('Waiting up to %d seconds for %d test outputs...', timeout_seconds, max_files)
        self.restart_observer_if_needed()
        wait_start_time = time.perf_counter()
        for i in range(0, max_files):
            # Note: The timing on Event.wait() is inaccurate
            self.done_event.wait(timeout_seconds)
            self.done_event.clear()
            current_time = time.perf_counter()
            if timeout_seconds < (current_time - wait_start_time) or output_validator.validate():
                break
        self.observer.stop()
        self.observer.join()

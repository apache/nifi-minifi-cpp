import logging
import os
import subprocess

from os import listdir
from os.path import join

from .FileOutputValidator import FileOutputValidator


class MultiFileOutputValidator(FileOutputValidator):
    """
    Validates the content of multiple files in the given directory, also verifying that the old files are not rewritten.
    """

    def __init__(self, expected_file_count, subdir=''):
        self.valid = False
        self.expected_file_count = expected_file_count
        self.subdir = subdir
        self.file_timestamps = dict()

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir, self.subdir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        listing = listdir(full_dir)
        if not listing:
            return self.valid

        for out_file_name in listing:
            logging.info("name:: %s", out_file_name)

            full_path = join(full_dir, out_file_name)
            if not os.path.isfile(full_path):
                return self.valid

            with open(full_path, 'r') as out_file:
                contents = out_file.read()
                logging.info("dir %s -- name %s", full_dir, out_file_name)
                logging.info("expected file count %d -- current file count %d", self.expected_file_count, len(self.file_timestamps))

                if full_path in self.file_timestamps and self.file_timestamps[full_path] != os.path.getmtime(full_path):
                    logging.error("Last modified timestamp changed for %s", full_path)
                    self.valid = False
                    return self.valid

                self.file_timestamps[full_path] = os.path.getmtime(full_path)
                logging.info("New file added %s", full_path)

                if len(self.file_timestamps) == self.expected_file_count:
                    self.valid = True
                    return self.valid

        return self.valid

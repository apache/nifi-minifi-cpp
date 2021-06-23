import logging
import os

from os import listdir
from os.path import join

from .FileOutputValidator import FileOutputValidator

class SingleOrMoreFileOutputValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_content, subdir=''):
        self.valid = False
        self.expected_content = expected_content
        self.subdir = subdir

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir, self.subdir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        self.valid = 0 < self.num_files_matching_content_in_dir(full_dir, self.expected_content)
        return self.valid

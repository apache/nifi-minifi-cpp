import logging
import os

from os import listdir
from os.path import join

from .FileOutputValidator import FileOutputValidator

class SingleFileOutputValidator(FileOutputValidator):
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

        listing = listdir(full_dir)
        if listing:
            for l in listing:
                logging.info("name:: %s", l)
            out_file_name = listing[0]
            full_path = join(full_dir, out_file_name)
            if not os.path.isfile(full_path):
                return self.valid

            with open(full_path, 'r') as out_file:
                contents = out_file.read()
                logging.info("dir %s -- name %s", full_dir, out_file_name)
                logging.info("expected %s -- content %s", self.expected_content, contents)

                if self.expected_content in contents:
                    self.valid = True

        return self.valid

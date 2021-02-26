import logging
import os

from os import listdir
from os.path import join

from .FileOutputValidator import FileOutputValidator
from ..core.HashUtils import md5

class SingleFileContentHashValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_md5_hash, subdir=''):
        self.valid = False
        self.expected_md5_hash = expected_md5_hash
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
            logging.info("dir %s -- name %s", full_dir, out_file_name)
            full_path = join(full_dir, out_file_name)
            if not os.path.isfile(full_path):
                return self.valid
            
            actual_md5_hash = md5(full_path)
            logging.info("expected hash: %s -- actual: %s", self.expected_md5_hash, actual_md5_hash)
            if self.expected_md5_hash == actual_md5_hash:
                self.valid = True

        return self.valid

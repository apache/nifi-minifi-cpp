import logging
import os

from os import listdir
from os.path import join

from .FileOutputValidator import FileOutputValidator

class NumFileRangeValidator(FileOutputValidator):

    def __init__(self, min_files, max_files, subdir=''):
        self.valid = False
        self.min_files = min_files
        self.max_files = max_files
        self.subdir = subdir

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir, self.subdir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        num_files = self.get_num_files(full_dir)
        self.valid = self.min_files < num_files and num_files < self.max_files
        return self.valid

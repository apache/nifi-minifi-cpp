import logging
import os

from .FileOutputValidator import FileOutputValidator


class NoFileOutPutValidator(FileOutputValidator):
    """
    Validates if no flowfiles were transferred
    """
    def __init__(self, subdir=''):
        self.valid = False
        self.subdir = subdir

    def validate(self, dir=''):
        self.valid = False
        full_dir = os.path.join(self.output_dir, self.subdir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        self.valid = (0 == self.get_num_files(full_dir))
        return self.valid

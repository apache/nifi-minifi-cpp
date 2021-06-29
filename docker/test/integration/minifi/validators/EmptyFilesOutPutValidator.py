import logging
import os

from os import listdir

from .FileOutputValidator import FileOutputValidator


class EmptyFilesOutPutValidator(FileOutputValidator):

    """
    Validates if all the files in the target directory are empty and at least one exists
    """
    def __init__(self):
        self.valid = False

    def validate(self, dir=''):

        if self.valid:
            return True

        full_dir = self.output_dir + dir
        logging.info("Output folder: %s", full_dir)
        listing = listdir(full_dir)
        if listing:
            self.valid = 0 < self.get_num_files(full_dir) and all(os.path.getsize(os.path.join(full_dir, x)) == 0 for x in listing)

        return self.valid

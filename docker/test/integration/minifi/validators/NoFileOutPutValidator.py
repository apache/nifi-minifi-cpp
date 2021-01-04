import logging

from os import listdir

from .FileOutputValidator import FileOutputValidator

class NoFileOutPutValidator(FileOutputValidator):
    """
    Validates if no flowfiles were transferred
    """
    def __init__(self):
        self.valid = False

    def validate(self, dir=''):

        if self.valid:
            return True

        full_dir = self.output_dir + dir
        logging.info("Output folder: %s", full_dir)
        listing = listdir(full_dir)

        self.valid = not bool(listing)

        return self.valid

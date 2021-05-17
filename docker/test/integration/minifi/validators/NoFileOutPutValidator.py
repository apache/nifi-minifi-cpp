import logging
import os

from .FileOutputValidator import FileOutputValidator


class NoFileOutPutValidator(FileOutputValidator):
    """
    Validates if no flowfiles were transferred
    """
    def __init__(self):
        self.valid = False

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        self.valid = (0 == self.get_num_files(full_dir))
        return self.valid

import logging
import os

from .FileOutputValidator import FileOutputValidator


class NoContentCheckFileNumberValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, num_files_expected):
        self.valid = False
        self.num_files_expected = num_files_expected

    def validate(self):
        self.valid = False
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return self.valid

        self.valid = self.num_files_expected == self.get_num_files(full_dir)
        return self.valid

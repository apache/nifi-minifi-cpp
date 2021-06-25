import logging
import os

from .FileOutputValidator import FileOutputValidator


class NoContentCheckFileNumberValidator(FileOutputValidator):
    """
    Validates the number of files created without content validation.
    """

    def __init__(self, num_files_expected):
        self.num_files_expected = num_files_expected

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        return self.num_files_expected == self.get_num_files(full_dir)

import logging
import os

from .FileOutputValidator import FileOutputValidator


class SingleOrMultiFileOutputValidator(FileOutputValidator):
    """
    Validates the content of a single or multiple files in the given directory.
    """

    def __init__(self, expected_content):
        self.expected_content = expected_content

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        return 0 < self.num_files_matching_content_in_dir(full_dir, self.expected_content)

import logging
import os

from .FileOutputValidator import FileOutputValidator
from ..core.HashUtils import md5


class SingleFileContentHashValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """
    def __init__(self, expected_md5_hash):
        self.expected_md5_hash = expected_md5_hash

    def file_matches_expected_hash(self, dir_path):
        listing = os.listdir(dir_path)
        if not listing:
            return 0
        for file_name in listing:
            full_path = os.path.join(dir_path, file_name)
            if not os.path.isfile(full_path):
                continue

            actual_md5_hash = md5(full_path)
            logging.info("expected hash: %s -- actual: %s", self.expected_md5_hash, actual_md5_hash)
            if self.expected_md5_hash == actual_md5_hash:
                return True
        return False

    def validate(self):
        logging.info("Output folder: %s", self.output_dir)

        if not os.path.isdir(self.output_dir):
            return False

        return self.get_num_files(self.output_dir) == 1 and self.file_matches_expected_hash(self.output_dir)

import logging
import os

from .FileOutputValidator import FileOutputValidator


class NoFileOutPutValidator(FileOutputValidator):
    """
    Validates if no flowfiles were transferred
    """
    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        return os.path.isdir(full_dir) and 0 == self.get_num_files(full_dir)

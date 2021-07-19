import logging
import os
import json

from .FileOutputValidator import FileOutputValidator


class SingleJSONFileOutputValidator(FileOutputValidator):
    """
    Validates the content of a single file in the given directory.
    """

    def __init__(self, expected_content):
        self.expected_content = json.loads(expected_content)

    def file_matches_json_content(self, dir_path, expected_json_content):
        listing = os.listdir(dir_path)
        if not listing:
            return 0
        for file_name in listing:
            full_path = os.path.join(dir_path, file_name)
            if not os.path.isfile(full_path):
                continue
            with open(full_path, 'r') as out_file:
                file_json_content = json.loads(out_file.read())
                return file_json_content == expected_json_content
        return False

    def validate(self):
        full_dir = os.path.join(self.output_dir)
        logging.info("Output folder: %s", full_dir)

        if not os.path.isdir(full_dir):
            return False

        return self.get_num_files(full_dir) == 1 and self.file_matches_json_content(full_dir, self.expected_content)

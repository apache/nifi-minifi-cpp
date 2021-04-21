import logging
import os

from os import listdir
from os.path import join

from .OutputValidator import OutputValidator


class FileOutputValidator(OutputValidator):
    def set_output_dir(self, output_dir):
        self.output_dir = output_dir

    @staticmethod
    def num_files_matching_content_in_dir(dir_path, expected_content):
        listing = listdir(dir_path)
        if not listing:
            return 0
        files_of_matching_content_found = 0
        for file_name in listing:
          full_path = join(dir_path, file_name)
          if not os.path.isfile(full_path):
              continue
          with open(full_path, 'r') as out_file:
              contents = out_file.read()
              logging.info("dir %s -- name %s", dir_path, file_name)
              logging.info("expected content: %s -- actual: %s, match: %r", expected_content, contents, expected_content == contents)
              if expected_content in contents:
                  files_of_matching_content_found += 1
        return files_of_matching_content_found

    @staticmethod
    def get_num_files(dir_path):
        listing = listdir(dir_path)
        logging.info("Num files in %s: %d", dir_path, len(listing))
        if not listing:
            return 0
        files_found = 0
        for file_name in listing:
            full_path = join(dir_path, file_name)
            if os.path.isfile(full_path):
              logging.info("Found output file in %s: %s", dir_path, file_name)
              files_found += 1
        return files_found

    def validate(self, dir=''):
        pass

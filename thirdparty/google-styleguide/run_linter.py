import argparse
import os
import cpplint

parser = argparse.ArgumentParser()
parser.add_argument('-i', '--includePaths', nargs="+", help='Run linter check in these directories')
parser.add_argument('-q', '--quiet', action='store_true', help='Don\'t print anything if no errors are found.')
args = parser.parse_args()

list_of_files = list()
for include_path in args.includePaths:
  for (dir_path, dir_names, file_names) in os.walk(include_path):
    for file_name in file_names:
      if (".h" in file_name) or (".cpp" in file_name):
        list_of_files += [os.path.join(dir_path, file_name)]

script_dir = os.path.dirname(os.path.realpath(__file__))
repository_path = os.path.abspath(os.path.join(script_dir, os.pardir, os.pardir))

arg_list = list()
arg_list.append("--linelength=200")
arg_list.append("--repository=" + repository_path)
if (args.quiet):
  arg_list.append("--quiet")

cpplint.main(arg_list + list_of_files)

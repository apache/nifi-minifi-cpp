import argparse
import multiprocessing
import os
import cpplint
from multiprocessing import Pool
import sys
import math

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
if args.quiet:
    arg_list.append("--quiet")


def cpplint_main_wrapper(file_list):
    try:
        cpplint.main(arg_list + file_list)
        return 0
    except SystemExit as err:
        return err.code

    return 1


if __name__ == '__main__':
    # break up list_of_files to ~equal chunks
    chunk_num = multiprocessing.cpu_count()
    chunk_size = math.ceil((len(list_of_files) / chunk_num))
    chunks = []
    chunk_begin = 0
    chunk_end = chunk_size
    for chunk_cnt in range(chunk_num):
        chunks.append(list_of_files[chunk_begin:chunk_end])
        chunk_begin += chunk_size
        if chunk_begin >= len(list_of_files):
            break
        chunk_end += chunk_size
        if chunk_end > len(list_of_files):
            chunk_end = len(list_of_files)
    pool = Pool(processes=chunk_num)
    # execute in parallel
    map_res = pool.map(cpplint_main_wrapper, chunks)
    pool.close()
    pool.join()
    # pass failure exit code if one has failed
    sys.exit(max(map_res))

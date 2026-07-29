# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import argparse
import multiprocessing
import os
import cpplint
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


def create_chunks(chunk_size, items):
    """breaks up items to equal chunks, with last being shorter if items are not evenly divisible by chunk_size"""
    chunk_list = []
    chunk_begin = 0
    chunk_end = chunk_size
    for chunk_cnt in range(chunk_num):
        chunk_list.append(items[chunk_begin:chunk_end])
        chunk_begin += chunk_size
        if chunk_begin >= len(list_of_files):
            break
        chunk_end += chunk_size
        if chunk_end > len(list_of_files):
            chunk_end = len(list_of_files)
    return chunk_list


if __name__ == '__main__':
    chunk_num = multiprocessing.cpu_count()
    chunk_max_size = math.ceil(len(list_of_files) / chunk_num)
    chunks = create_chunks(chunk_max_size, list_of_files)
    pool = multiprocessing.Pool(processes=chunk_num)
    # execute in parallel
    map_res = pool.map(cpplint_main_wrapper, chunks)
    pool.close()
    pool.join()
    # pass failure exit code if one has failed
    sys.exit(max(map_res))

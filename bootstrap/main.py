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
import tempfile

import argparse
import pathlib

from cli import main_menu, do_one_click_build
from minifi_option import parse_minifi_options
from package_manager import get_package_manager

if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as cmake_cache_dir:
        parser = argparse.ArgumentParser()
        parser.add_argument('--noconfirm', action="store_true", default=False,
                            help="Bypass any and all “Are you sure?” messages.")
        parser.add_argument('--minifi_options', default="", help="Overrides the default minifi options during the "
                                                                 "initial parsing")
        parser.add_argument('--cmake_options', default="", help="Appends this to the final cmake command")
        parser.add_argument('--skip_compiler_install', action="store_true", default=False,
                            help="Skips the installation of the default compiler")
        parser.add_argument('--noninteractive', action="store_true", default=False,
                            help="Initiates the one click build")
        args = parser.parse_args()
        no_confirm = args.noconfirm or args.noninteractive

        package_manager = get_package_manager(no_confirm)
        if not args.skip_compiler_install:
            compiler_override = package_manager.install_compiler()
        else:
            compiler_override = ""
        cmake_options_for_parsing = " ".join(filter(None, [args.minifi_options, compiler_override]))
        cmake_options_for_cmake = " ".join(filter(None, [args.cmake_options, compiler_override]))

        path = pathlib.Path(__file__).parent.resolve() / '..' / "cmake" / "MiNiFiOptions.cmake"

        minifi_options = parse_minifi_options(str(path.as_posix()), cmake_options_for_parsing, package_manager, cmake_cache_dir)
        minifi_options.no_confirm = no_confirm
        minifi_options.set_cmake_override(cmake_options_for_cmake)

        if args.noninteractive:
            do_one_click_build(minifi_options, package_manager)
        else:
            main_menu(minifi_options, package_manager)

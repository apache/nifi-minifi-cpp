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

from __future__ import annotations

from typing import Dict, Set

from minifi_option import MinifiOptions
from package_manager import PackageManager
import platform


def _create_system_dependencies(minifi_options: MinifiOptions) -> Dict[str, Set[str]]:
    system_dependencies = {'patch': {'patch'}, 'make': {'make'}, 'perl': {'perl'}, 'git': {'git'}, 'bison': {'bison'}, 'flex': {'flex'}, 'm4': {'m4'}}
    if minifi_options.is_enabled("ENABLE_LIBARCHIVE"):
        system_dependencies['libarchive'] = {'libarchive'}
    if minifi_options.is_enabled("ENABLE_SQL"):
        system_dependencies['automake'] = {'automake'}
        system_dependencies['autoconf'] = {'autoconf'}
        system_dependencies['libtool'] = {'libtool'}
    if minifi_options.is_enabled("ENABLE_PYTHON_SCRIPTING"):
        system_dependencies['python'] = {'python'}
    if platform.system() == "Windows":
        system_dependencies['wixtoolset'] = {'wixtoolset'}
    return system_dependencies


def install_required(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    return package_manager.install(_create_system_dependencies(minifi_options))

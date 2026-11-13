# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

from __future__ import annotations

import platform

from minifi_option import MinifiOptions
from package_manager import PackageManager


def _create_system_dependencies(minifi_options: MinifiOptions) -> dict[str, set[str]]:
    system_dependencies = {
        "patch": {"patch"},
        "make": {"make"},
        "perl": {"perl"},
        "git": {"git"},
        "bison": {"bison"},
        "flex": {"flex"},
        "m4": {"m4"},
    }
    if minifi_options.is_enabled("ENABLE_LIBARCHIVE"):
        system_dependencies["libarchive"] = {"libarchive"}
    if minifi_options.is_enabled("ENABLE_SQL"):
        system_dependencies["automake"] = {"automake"}
        system_dependencies["autoconf"] = {"autoconf"}
        system_dependencies["libtool"] = {"libtool"}
    if minifi_options.is_enabled("ENABLE_PYTHON_SCRIPTING"):
        system_dependencies["python"] = {"python"}
    if platform.system() == "Windows":
        system_dependencies["wixtoolset"] = {"wixtoolset"}
    return system_dependencies


def install_required(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    return package_manager.install(_create_system_dependencies(minifi_options))

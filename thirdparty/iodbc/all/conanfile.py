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

import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, rm, rmdir
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.layout import basic_layout

required_conan_version = ">=2.0"


class IodbcConan(ConanFile):
    name = "iodbc"
    description = "iODBC is a platform-independent ODBC driver manager (LGPL, with linking exception)"
    license = "LGPL-2.0-or-later"
    url = "https://github.com/openlink/iODBC"
    homepage = "https://www.iodbc.org/"
    topics = ("odbc", "driver-manager", "database")
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "fPIC": [True, False],
    }
    default_options = {
        "fPIC": True,
    }

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        basic_layout(self, src_folder="src")

    def validate(self):
        if self.settings.os == "Windows":
            raise ConanInvalidConfiguration("iodbc does not support Windows; use the system ODBC driver manager instead")

    def build_requirements(self):
        self.tool_requires("libtool/2.4.7")
        self.tool_requires("autoconf/2.71")
        self.tool_requires("automake/1.16.5")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = AutotoolsToolchain(self)
        if self.options.fPIC:
            tc.configure_args.append("--with-pic")
        tc.configure_args.append("--enable-static")
        tc.configure_args.append("--disable-shared")
        # Matches the from-source build (see cmake/BundledIodbc.cmake)
        tc.extra_cflags.append("-std=gnu17")
        tc.generate()

    def build(self):
        apply_conandata_patches(self)
        self.run("./autogen.sh", cwd=self.source_folder)
        autotools = Autotools(self)
        autotools.configure(build_script_folder=self.source_folder)
        autotools.make()

    def package(self):
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYING*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        # Expose iODBC as a drop-in ODBC provider so consumers relying on
        # find_package(ODBC) / ODBC::ODBC (e.g. SOCI) resolve to it.
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "ODBC")
        self.cpp_info.set_property("cmake_target_name", "ODBC::ODBC")
        self.cpp_info.libs = ["iodbc"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread", "dl"]

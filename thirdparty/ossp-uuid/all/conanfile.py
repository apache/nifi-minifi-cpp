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
from conan.tools.files import copy, export_conandata_patches, get, rm, rmdir
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.layout import basic_layout

required_conan_version = ">=2.0"


class OsspUuidConan(ConanFile):
    name = "ossp-uuid"
    description = "OSSP uuid is an ISO-C:1999 API and CLI for generating DCE, ISO/IEC and RFC 4122 Universally Unique Identifiers (UUID)"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "http://www.ossp.org/pkg/lib/uuid/"
    topics = ("uuid", "unique-identifier", "guid")
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
        # ossp-uuid only supports in-source builds
        self.folders.build = self.folders.source

    def validate(self):
        if self.settings.os == "Windows":
            raise ConanInvalidConfiguration("ossp-uuid does not support Windows")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = AutotoolsToolchain(self)
        tc.configure_args.append("--with-cxx")
        tc.configure_args.append("--without-perl")
        tc.configure_args.append("--without-php")
        tc.configure_args.append("--without-pgsql")
        tc.configure_args.append("--disable-shared")
        tc.configure_args.append("--enable-static")
        if self.settings.build_type == "Debug":
            tc.configure_args.append("--enable-debug=yes")
        tc.generate()

    def build(self):
        for patch_data in self.conan_data.get("patches", {}).get(self.version, []):
            patch_file = os.path.join(self.export_sources_folder, patch_data["patch_file"])
            self.run(f'patch -p1 -N -i "{patch_file}"', cwd=self.source_folder)
        autotools = Autotools(self)
        autotools.configure(build_script_folder=self.source_folder)
        autotools.make()

    def package(self):
        copy(self, "README", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "bin"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "ossp-uuid")

        self.cpp_info.components["libuuid"].set_property("cmake_target_name", "OSSP::libuuid")
        self.cpp_info.components["libuuid"].libs = ["uuid"]

        self.cpp_info.components["libuuid++"].set_property("cmake_target_name", "OSSP::libuuid++")
        self.cpp_info.components["libuuid++"].libs = ["uuid++"]
        self.cpp_info.components["libuuid++"].requires = ["libuuid"]

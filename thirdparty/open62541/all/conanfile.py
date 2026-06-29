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

# Recipe based on https://github.com/conan-io/conan-center-index/blob/master/recipes/open62541/all/conanfile.py
from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeToolchain, CMakeDeps
from conan.tools.scm import Version
from conan.tools.files import apply_conandata_patches, collect_libs, export_conandata_patches, copy, rm, rmdir, get
from conan.errors import ConanInvalidConfiguration
import glob
import os

required_conan_version = ">=2.0"


class Open62541Conan(ConanFile):
    name = "open62541"
    description = "open62541 is an open source and free implementation of OPC UA " \
                  "(OPC Unified Architecture) written in the common subset of the " \
                  "C99 and C++98 languages. The library is usable with all major " \
                  "compilers and provides the necessary tools to implement dedicated " \
                  "OPC UA clients and servers, or to integrate OPC UA-based communication " \
                  "into existing applications. open62541 library is platform independent. " \
                  "All platform-specific functionality is implemented via exchangeable " \
                  "plugins. Plugin implementations are provided for the major operating systems."
    license = ("MPL-2.0", "CC0-1.0")
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://open62541.org/"
    topics = (
        "opc ua", "sdk", "server/client", "c", "iec-62541",
        "industrial automation", "tsn", "time sensitive networks", "publish-subscribe", "pubsub"
    )

    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "fPIC": [True, False],
        "shared": [True, False],
    }
    default_options = {
        "fPIC": True,
        "shared": False,
    }

    short_paths = True

    def export_sources(self):
        export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires("openssl/[>=1.1 <4]")
        self.requires("libxml2/[>=2.12.5 <3]")

    def validate(self):
        if self.settings.compiler == "clang" and Version(self.settings.compiler.version) == "9":
            raise ConanInvalidConfiguration(
                f"{self.ref} does not support Clang version {self.settings.compiler.version}")

        if self.settings.compiler == "clang":
            if Version(self.settings.compiler.version) < "5":
                raise ConanInvalidConfiguration(
                    "Older clang compiler version than 5.0 are not supported")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        tc = CMakeToolchain(self)

        version = Version(self.version)
        tc.variables["OPEN62541_VER_MAJOR"] = version.major
        tc.variables["OPEN62541_VER_MINOR"] = version.minor
        tc.variables["OPEN62541_VER_PATCH"] = version.patch
        tc.variables["UA_ENABLE_ENCRYPTION"] = True
        tc.variables["UA_ENABLE_ENCRYPTION_OPENSSL"] = True

        tc.generate()
        tc = CMakeDeps(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    @property
    def _tools_subfolder(self):
        return os.path.join(self.source_folder, "tools")

    @property
    def _module_subfolder(self):
        return os.path.join("lib", "cmake", "open62541")

    @property
    def _module_file_rel_path(self):
        return os.path.join(self._module_subfolder, "open62541Macros.cmake")

    def package(self):
        licenses_dir = os.path.join(self.package_folder, "licenses")
        copy(self, "LICENSE", src=self.source_folder, dst=licenses_dir)
        copy(self, "LICENSE-CC0", src=self.source_folder, dst=licenses_dir)
        cmake = CMake(self)
        cmake.install()

        rm(self, '*.pdb', os.path.join(self.package_folder, "bin"))
        rm(self, '*.pdb', os.path.join(self.package_folder, "lib"))

        for cmake_file in glob.glob(os.path.join(self.package_folder, self._module_subfolder, "*")):
            if not cmake_file.endswith(self._module_file_rel_path):
                os.remove(cmake_file)

        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

        tools_dir = os.path.join(self.package_folder, "res", "tools")
        copy(self, "generate_*.py", src=self._tools_subfolder, dst=tools_dir)
        copy(self, "nodeset_compiler/*", src=self._tools_subfolder, dst=tools_dir)

    @staticmethod
    def _chmod_plus_x(filename):
        if os.name == 'posix':
            os.chmod(filename, os.stat(filename).st_mode | 0o111)

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        self.cpp_info.includedirs = ["include"]

        # required for creating custom servers from ua-nodeset
        self.conf_info.define("user.open62541:tools_dir", os.path.join(
            self.package_folder, "res", "tools").replace("\\", "/"))
        self._chmod_plus_x(os.path.join(self.package_folder,
                           "res", "tools", "generate_nodeid_header.py"))

        self.cpp_info.includedirs.append(
            os.path.join("include", "open62541", "plugin"))

        if self.settings.os == "Windows":
            self.cpp_info.system_libs.append("ws2_32")
            self.cpp_info.system_libs.append("iphlpapi")
        elif self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.extend(["pthread", "m", "rt"])

        self.cpp_info.builddirs.append(self._module_subfolder)
        self.cpp_info.set_property("cmake_build_modules", [
                                   self._module_file_rel_path])

# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import os

from conan import ConanFile
from conan.tools.build import build_jobs
from conan.tools.files import copy, get
from conan.tools.layout import basic_layout
from conan.tools.microsoft import is_msvc, VCVars

required_conan_version = ">=2.0"


class OpenSSLFipsConan(ConanFile):
    name = "openssl-fips"
    description = "OpenSSL FIPS provider module (fips.so / fips.dll) built from FIPS-validated OpenSSL sources"
    license = "Apache-2.0"
    homepage = "https://github.com/openssl/openssl"
    topics = ("openssl", "fips", "ssl", "tls", "encryption", "security")
    package_type = "shared-library"
    settings = "os", "arch", "compiler", "build_type"

    def layout(self):
        basic_layout(self, src_folder="src")
        self.folders.build = self.folders.source

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        if is_msvc(self):
            VCVars(self).generate()

    @property
    def _configure_flags(self):
        return [
            "no-tests",
            "no-capieng",
            "no-legacy",
            "no-ssl",
            "no-engine",
            "enable-fips",
        ]

    def build(self):
        flags = " ".join(self._configure_flags)
        prefix_args = f'"--prefix={self.package_folder}" "--openssldir={self.package_folder}"'
        if is_msvc(self):
            self.run(f"perl Configure {flags} {prefix_args}", cwd=self.source_folder)
            self.run("nmake", cwd=self.source_folder)
        else:
            self.run(f'./Configure "CFLAGS=-fPIC" "CXXFLAGS=-fPIC" {flags} {prefix_args}', cwd=self.source_folder)
            self.run(f"make -j{build_jobs(self)}", cwd=self.source_folder)

    def package(self):
        copy(self, "LICENSE.txt", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        self.run("nmake install_fips" if is_msvc(self) else "make install_fips", cwd=self.source_folder)

    def package_info(self):
        self.cpp_info.libs = []
        self.cpp_info.includedirs = []
        self.cpp_info.set_property("cmake_file_name", "openssl-fips")

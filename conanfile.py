from conan import ConanFile, tools
from conan.tools.cmake import CMake
import os, subprocess, shutil
from glob import glob

import shlex
import subprocess
from conan.internal import check_duplicated_generator
from conan.tools.env import Environment
from conan.tools.env.virtualrunenv import runenv_from_cpp_info
from conan.tools.cmake import CMakeToolchain

required_conan_version = ">=1.54 <2.0 || >=2.0.14"

# Does conan have argparse?
# https://github.com/p-ranav/argparse/archive/refs/tags/v3.0.tar.gz

shared_requires = (
    'abseil/20230125.3',
    'argparse/3.0'
)

linux_requires = (

)

window_requires = (

)

shared_options = {
    "shared": [True, False],
    "fPIC": [True, False],
}

shared_default_options = {
    "shared": False,
    "fPIC": True,
}

class MiNiFiCppMain(ConanFile):
    name = "minifi-cpp-main"
    version = "0.15.0"
    license = "Apache-2.0"
    requires = shared_requires
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = shared_options
    default_options = shared_default_options
    url = "https://nifi.apache.org/projects/minifi/"

    def configure(self):
        pass

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["USE_CONAN_PACKAGER"] = "ON"
        tc.cache_variables["USE_CMAKE_FETCH_CONTENT"] = "OFF"
        tc.generate()

    def requirements(self):
        if self.settings.os == "Windows":
            for require in window_requires:
                self.requires.add(require)
        else:
            for require in linux_requires:
                self.requires.add(require)

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

conancenter_shared_requires = (
    'abseil/20230125.3',
    'argparse/3.0',
    'asio/1.30.2',
    'catch2/3.5.4'
)

# packages not available on conancenter
    # TODO (JG): Add conan recipes for building these packages from source, 
    # later upload to github packages, so we can maintain our own prebuilt packages
    # format: '{conan_package}/{version}@{user}/{channel}'
    # version: commit hash or official tag version
    # user: minifi, channels: stable, testing or dev, etc
# - bustache: https://github.com/jamboree/bustache
github_pcks_shared_requires = (
    'bustache/1a6d442@minifi/dev'
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
    requires = conancenter_shared_requires
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

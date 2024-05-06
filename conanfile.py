from conan import ConanFile, tools
from conan.tools.cmake import CMake
import os, subprocess, shutil
from glob import glob

import shlex
import subprocess
from conan.internal import check_duplicated_generator
from conan.tools.env import Environment
from conan.tools.env.virtualrunenv import runenv_from_cpp_info
from conan.tools.cmake import CMakeToolchain, CMake

required_conan_version = ">=1.54 <2.0 || >=2.1.0"

# conan prebuilt binary conan packages for minifi core, mainexe, standard processors
minifi_core_external_libraries = (
    'libsodium/cci.20220430',
    'zlib/1.3.1',
    'spdlog/1.14.0',
    'yaml-cpp/0.8.0',
    'gsl-lite/0.41.0',
    'date/3.0.1',
    'tz/2023c',
    'expected-lite/0.6.3',
    'magic_enum/0.9.5',
    'abseil/20230125.3',
    'civetweb/1.16',
    'libcurl/8.6.0',
    'openssl/3.2.1',
    'asio/1.30.2',
    'range-v3/0.12.0',
    'libxml2/2.12.6',
    # 'uthash/2.3.0',
    # 'concurrentqueue/1.0.4',
    # 'rapidjson/cci.20230929',
)

# conan non prebuilt binary conan packages for minifi core, so we have source list
minifi_core_external_source_libraries = (
    'catch2/3.5.4',
)

# minifi_extension_external_libraries = (
    # 'argparse/3.0',
# )

shared_requires = minifi_core_external_libraries 
shared_requires += minifi_core_external_source_libraries 

# packages not available on conancenter or need to be prebuilt with MiNiFi C++ specific patches, etc
    # TODO (JG): Add conan recipes for building these packages from source, 
    # later upload to github packages, so we can maintain our own prebuilt packages
    # format: '{conan_package}/{version}@{user}/{channel}'
    # version: commit hash or official tag version
    # user: minifi, channels: stable, testing or dev, etc
github_pcks_shared_requires = (
    'rocksdb/8.10.2@minifi/dev', # nifi-minifi-cpp/thirdparty/rocksdb/all
#     'bustache/1a6d442@minifi/dev', # - bustache: https://github.com/jamboree/bustache
#     'ossp-uuid/1.6.2@minifi/dev',
#     'openwsman/2.7.2@minifi/dev',
    # 'bzip2/1.0.8@minifi/dev',
)

# conan packages for minifi core & standard-processor gtests
minifi_core_gtests_external_libraries = github_pcks_shared_requires

shared_requires += minifi_core_gtests_external_libraries

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

    def requirements(self):
        if self.settings.os == "Windows":
            for require in window_requires:
                self.requires.add(require)
        elif self.settings.os == "Linux":
            for require in linux_requires:
                self.requires.add(require)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["USE_CONAN_PACKAGER"] = "ON"
        tc.cache_variables["USE_CMAKE_FETCH_CONTENT"] = "OFF"

        # NEEDED for MiNiFi C++ Core, MainExe, Standard Processors
        tc.cache_variables["ENABLE_OPENWSMAN"] = "ON"
        tc.cache_variables["MINIFI_OPENSSL"] = "ON"
        tc.cache_variables["ENABLE_CIVET"] = "ON"
        tc.cache_variables["ENABLE_CURL"] = "ON"

        # NEEDED for MiNiFi C++ GTESTS
        tc.cache_variables["SKIP_TESTS"] = "OFF"
        tc.cache_variables["ENABLE_EXPRESSION_LANGUAGE"] = "ON"
            # NOTE: Realized Rocksdb depends on ZLib, BZip2, Zstd, Lz4 on standalone CMake and need to be explicitly ON
                # whereas for rocksdb conan package, these lib deps are already handled by conan package
        tc.cache_variables["ENABLE_BZIP2"] = "ON"
        tc.cache_variables["ENABLE_ROCKSDB"] = "ON"
        tc.cache_variables["BUILD_ROCKSDB"] = "ON"

        # NEEDED for MiNiFi C++ Extensions
        tc.cache_variables["ENABLE_OPS"] = "OFF"
        tc.cache_variables["ENABLE_JNI"] = "OFF"
        tc.cache_variables["ENABLE_OPC"] = "OFF"
        tc.cache_variables["ENABLE_NANOFI"] = "OFF"

        if self.settings.os == "Windows":
            tc.cache_variables["ENABLE_WEL"] = "OFF"
            tc.cache_variables["ENABLE_PDH"] = "OFF"
            tc.cache_variables["ENABLE_SMB"] = "OFF"
        elif self.settings.os == "Linux":
            tc.cache_variables["ENABLE_SYSTEMD"] = "OFF"
            tc.cache_variables["ENABLE_PROCFS"] = "OFF"

        tc.cache_variables["ENABLE_ALL"] = "OFF"

        tc.cache_variables["ENABLE_LIBARCHIVE"] = "OFF"
        tc.cache_variables["ENABLE_LZMA"] = "OFF"

        tc.cache_variables["ENABLE_GPS"] = "OFF"
        tc.cache_variables["ENABLE_COAP"] = "OFF"
        tc.cache_variables["ENABLE_SQL"] = "OFF"
        tc.cache_variables["ENABLE_MQTT"] = "OFF"
        tc.cache_variables["ENABLE_PCAP"] = "OFF"
        tc.cache_variables["ENABLE_LIBRDKAFKA"] = "OFF"
        tc.cache_variables["ENABLE_LUA_SCRIPTING"] = "OFF"
        tc.cache_variables["ENABLE_PYTHON_SCRIPTING"] = "OFF"
        tc.cache_variables["ENABLE_SENSORS"] = "OFF"
        tc.cache_variables["ENABLE_USB_CAMERA"] = "OFF"
        tc.cache_variables["ENABLE_AWS"] = "OFF"
        tc.cache_variables["ENABLE_OPENCV"] = "OFF"
        tc.cache_variables["ENABLE_BUSTACHE"] = "OFF"
        tc.cache_variables["ENABLE_SFTP"] = "OFF"
        tc.cache_variables["ENABLE_AZURE"] = "OFF"
        tc.cache_variables["ENABLE_ENCRYPT_CONFIG"] = "OFF"
        tc.cache_variables["ENABLE_SPLUNK"] = "OFF"
        tc.cache_variables["ENABLE_ELASTICSEARCH"] = "OFF"
        tc.cache_variables["ENABLE_GCP"] = "OFF"
        tc.cache_variables["ENABLE_KUBERNETES"] = "OFF"
        tc.cache_variables["ENABLE_TEST_PROCESSORS"] = "OFF"
        tc.cache_variables["ENABLE_PROMETHEUS"] = "OFF"
        tc.cache_variables["ENABLE_GRAFANA_LOKI"] = "OFF"
        tc.cache_variables["ENABLE_GRPC_FOR_LOKI"] = "OFF"
        tc.cache_variables["ENABLE_CONTROLLER"] = "OFF"

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

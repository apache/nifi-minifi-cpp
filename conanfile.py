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
    'asio/1.30.2',
    'range-v3/0.12.0',
    'libxml2/2.12.6',
    # 'uthash/2.3.0',
    # 'concurrentqueue/1.0.4',
    # 'rapidjson/cci.20230929',
)

# conan non prebuilt binary conan packages for minifi core, so we have source list
minifi_core_external_source_libraries = (
    'catch2/3.5.4', # for conan 2.2.3, catch2 is not available as prebuilt binary package, so we built source downloaded by conan=center-index recipe
    'openssl/3.3.0@minifi/dev', # conan-center-index didnt have 3.3.0, so built from source by local conan recipe, needed by core-minifi, optional for libarchive
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
github_pcks_minifi_core_gtests_ext_libs = (
    'rocksdb/8.10.2@minifi/dev', # nifi-minifi-cpp/thirdparty/rocksdb/all
    'bzip2/1.0.8@minifi/dev', # need to check if there are any patches used
#     'bustache/1a6d442@minifi/dev', # - bustache: https://github.com/jamboree/bustache
#     'ossp-uuid/1.6.2@minifi/dev',
#     'openwsman/2.7.2@minifi/dev',

)

# conan packages for minifi core & standard-processor gtests
minifi_core_gtests_external_libraries = github_pcks_minifi_core_gtests_ext_libs

shared_requires += minifi_core_gtests_external_libraries

minifi_extension_external_libraries = (
)

# packages not available on conancenter or need to be prebuilt with MiNiFi C++ specific patches, etc
github_pcks_minifi_extension_ext_libs = (
    'mbedtls/2.16.3@minifi/dev', # needed to fix mbedtls packaging issue to allow for open62541 to integrate with it
    'open62541/1.3.3@minifi/dev', # open62541 expects version 1.3.3 with patches for minifi
    'xz_utils/5.2.5@minifi/dev', # xz_utils (liblzma) expects version 5.2.5 with patches and configure args for minifi
    'libarchive/3.4.2@minifi/dev' # libarchive with patches and configure args for minifi; need to add support for openssl enabled integration
)

shared_requires += minifi_extension_external_libraries

shared_requires += github_pcks_minifi_extension_ext_libs

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
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "enable_libarchive": [True, False],
        "minifi_extension_openssl": [True, False],
        "enable_lzma": [True, False],
        "enable_bzip2": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "enable_libarchive": True,
        "minifi_extension_openssl": False, # NOTE: libarchive integration with openssl is not working; filed a bug with conan-center-index
        "enable_lzma": True,
        "enable_bzip2": True,
    }
    url = "https://nifi.apache.org/projects/minifi/"

    def configure_libarchive(self):
        self.options["libarchive"].with_openssl = self.options.minifi_extension_openssl
        self.options["libarchive"].with_lzma = self.options.enable_lzma
        self.options["libarchive"].with_bzip2 = self.options.enable_bzip2

    def configure(self):
        if self.options.enable_libarchive:
            self.configure_libarchive()

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

        ##
        # NEEDED for MiNiFi C++ Core, MainExe, Standard Processors
        ##
        tc.cache_variables["ENABLE_OPENWSMAN"] = "ON"
        tc.cache_variables["MINIFI_OPENSSL"] = "ON"
        tc.cache_variables["ENABLE_CIVET"] = "ON"
        tc.cache_variables["ENABLE_CURL"] = "ON"

        ##
        # NEEDED for MiNiFi C++ GTESTS
        ##
        tc.cache_variables["SKIP_TESTS"] = "OFF"
        tc.cache_variables["ENABLE_EXPRESSION_LANGUAGE"] = "ON"
            # NOTE: Realized Rocksdb depends on ZLib, BZip2, Zstd, Lz4 on standalone CMake and need to be explicitly ON
                # whereas for rocksdb conan package, these lib deps are already handled by conan package
        tc.cache_variables["ENABLE_BZIP2"] = "ON"
        tc.cache_variables["ENABLE_ROCKSDB"] = "ON"
        tc.cache_variables["BUILD_ROCKSDB"] = "ON"

        ##
        # NEEDED for MiNiFi C++ Extensions
        ##
        tc.cache_variables["ENABLE_ALL"] = "OFF"

        tc.cache_variables["ENABLE_OPS"] = "ON"
        tc.cache_variables["ENABLE_JNI"] = "OFF" # Need to add Maven to my docker env: Could NOT find Maven (missing: MAVEN_EXECUTABLE); extensions jni CMakeLists.txt 29
        tc.cache_variables["ENABLE_OPC"] = "ON"
        tc.cache_variables["ENABLE_NANOFI"] = "ON"

        if self.settings.os == "Windows":
            tc.cache_variables["ENABLE_WEL"] = "OFF"
            tc.cache_variables["ENABLE_PDH"] = "OFF"
            tc.cache_variables["ENABLE_SMB"] = "OFF"
        elif self.settings.os == "Linux":
            tc.cache_variables["ENABLE_SYSTEMD"] = "ON"
            tc.cache_variables["ENABLE_PROCFS"] = "ON"

        tc.cache_variables["ENABLE_LIBARCHIVE"] = "ON" if self.options.enable_libarchive else "OFF"
        tc.cache_variables["ENABLE_LZMA"] = "ON" if self.options.enable_lzma else "OFF"

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

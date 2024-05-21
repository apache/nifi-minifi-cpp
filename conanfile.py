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
    # 'bzip2/1.0.8@minifi/dev', # create bzip2 conan package with patches for minifi
#     'bustache/1a6d442@minifi/dev', # - bustache: https://github.com/jamboree/bustache
#     'ossp-uuid/1.6.2@minifi/dev',
#     'openwsman/2.7.2@minifi/dev',

)

# conan packages for minifi core & standard-processor gtests
minifi_core_gtests_external_libraries = github_pcks_minifi_core_gtests_ext_libs

shared_requires += minifi_core_gtests_external_libraries

minifi_extension_external_libraries = (
    'odbc/2.3.11', # soci conan package uses unixodbc, might try openlink's iodbc if needed to build minifi
    'argparse/3.0', # needed for minifi controller
    'lua/5.4.6', 
    'sol2/3.3.0', # applied minifi patch to sol2, is a C++ library binding to Lua for advanced featuers & top performance
    'bzip2/1.0.8',
    'libuvc/0.0.7',
    'openjdk/21.0.2',
    'libpcap/1.10.1',
)

# packages not available on conancenter or need to be prebuilt with MiNiFi C++ specific patches, etc
github_pcks_minifi_extension_ext_libs = (
    'mbedtls/2.16.3@minifi/dev', # needed to fix mbedtls packaging issue to allow for open62541 to integrate with it
    'open62541/1.3.3@minifi/dev', # open62541 expects version 1.3.3 with patches for minifi
    'xz_utils/5.2.5@minifi/dev', # xz_utils (liblzma) expects version 5.2.5 with patches and configure args for minifi
    'libarchive/3.4.2@minifi/dev', # libarchive with patches and configure args for minifi; need to add support for openssl enabled integration
    'libcoap/4.2.1@minifi/dev', # updated libcoap 4.3.x conanfile.py to build for libcoap 4.2.1 needed by minifi
    'soci/4.0.1@minifi/dev', # updated soci conanfile.py to build soci 4.0.1 with sqlite patch needed by minifi
    'pcapplusplus/22.05@minifi/dev', # updated pcapplusplus conanfile.py to build pcapplusplus 22.05 needed by minifi
    # 'cpython/3.9.19@minifi/dev' # use python3.9+ conan package to match nifi custom python extensibility recommended python version
    # 'cpython/3.12.2@minifi/dev' # use python3.12.2 conan package to meet nifi custom python extensibility python version requirement
    # 'aws-sdk-cpp/1.9.234',
    'ffmpeg/4.4.4@minifi/dev', # opencv requires ffempg, ffempeg requires xz_utils
    'libtiff/4.6.0@minifi/dev', # opencv requires libtiff, libtiff requires xz_utils
    'opencv/4.8.1@minifi/dev',
    'libssh2/1.10.0@minifi/dev', # requires openssl and mbedtls, so use minifi ones
    'maven/3.9.6@minifi/dev', # updated maven conan package with MAVEN_EXECUTABLE env variable
    'librdkafka/1.9.2@minifi/dev', # updated librdkafka conan package CMake definition args similar to ExternalProject_Add
    'libsystemd/255@minifi/dev', # updated libsystemd to use minifi's xz_utils
    'grpc/1.54.3@minifi/dev', # updated grpc conan package to use minifi's openssl, libsystemd
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
    version = "0.99.0"
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
        tc.cache_variables["ENABLE_JNI"] = "ON" # for JNI extension, I saw for conan approach, it needs to be updated to use Py Maven Executable
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

        tc.cache_variables["ENABLE_GPS"] = "ON"
        tc.cache_variables["ENABLE_COAP"] = "ON"
        tc.cache_variables["ENABLE_SQL"] = "ON"
        tc.cache_variables["ENABLE_MQTT"] = "ON"
        tc.cache_variables["ENABLE_PCAP"] = "ON"
        tc.cache_variables["ENABLE_LIBRDKAFKA"] = "ON"
        tc.cache_variables["ENABLE_LUA_SCRIPTING"] = "ON"
        tc.cache_variables["ENABLE_PYTHON_SCRIPTING"] = "OFF"
        tc.cache_variables["ENABLE_SENSORS"] = "ON"
        tc.cache_variables["ENABLE_USB_CAMERA"] = "ON"
        tc.cache_variables["ENABLE_AWS"] = "OFF"
        tc.cache_variables["ENABLE_OPENCV"] = "ON"
        tc.cache_variables["ENABLE_BUSTACHE"] = "OFF"
        tc.cache_variables["ENABLE_SFTP"] = "ON"
        tc.cache_variables["ENABLE_AZURE"] = "OFF"
        tc.cache_variables["ENABLE_ENCRYPT_CONFIG"] = "ON"
        tc.cache_variables["ENABLE_SPLUNK"] = "ON"
        tc.cache_variables["ENABLE_ELASTICSEARCH"] = "ON"
        tc.cache_variables["ENABLE_GCP"] = "OFF"
        tc.cache_variables["ENABLE_KUBERNETES"] = "OFF"
        tc.cache_variables["ENABLE_TEST_PROCESSORS"] = "ON"
        tc.cache_variables["ENABLE_PROMETHEUS"] = "OFF"
        tc.cache_variables["ENABLE_GRAFANA_LOKI"] = "ON"
        tc.cache_variables["ENABLE_GRPC_FOR_LOKI"] = "ON"
        tc.cache_variables["ENABLE_CONTROLLER"] = "ON"

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

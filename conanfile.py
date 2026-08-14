#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import collect_libs, copy
from conan.errors import ConanException
import os
import shutil

required_conan_version = ">=2.0"

shared_requires = ("civetweb/1.16", "libxml2/2.15.3", "fmt/12.1.0", "spdlog/1.17.0", "catch2/3.15.0", "zstd/1.5.7", "sol2/3.5.0", "argparse/3.2", "libsodium/1.0.22", "gsl-lite/1.1.0",
                   "jsoncons/1.7.0", "json-schema-validator/2.4.0", "pugixml/1.16", "yaml-cpp/0.9.0", "range-v3/0.12.0", "magic_enum/0.9.8@minifi/develop", "date/3.0.4")

shared_sources = ("CMakeLists.txt", "libminifi/*", "extensions/*", "minifi_main/*", "behave_framework/*", "bin/*", "bootstrap/*", "cmake/*", "conf/*", "controller/*", "core-framework/*",
                  "docs/*", "encrypt-config/*", "etc/*", "examples/*", "extension-framework/*", "fips/*", "minifi-api/*", "packaging/*", "thirdparty/*", "docker/*", "LICENSE", "NOTICE",
                  "README.md", "C2.md", "CONAN.md", "CONFIGURE.md", "CONTRIBUTING.md", "CONTROLLERS.md", "EXPRESSIONS.md", "Extensions.md", "METRICS.md", "OPS.md", "PARAMETER_PROVIDERS.md",
                  "PROCESSORS.md", "SITE_TO_SITE.md", "ThirdParties.md", "Windows.md", "CPPLINT.cfg", "generateVersion.bat", "generateVersion.sh", "run_clang_tidy.sh", "run_flake8.sh",
                  "run_shellcheck.sh", "versioninfo.rc.in")


class MiNiFiCppMain(ConanFile):
    name = "minifi-cpp"
    version = "1.0.0"
    license = "Apache-2.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = {"shared": [True, False], "fPIC": [True, False], "custom_malloc": [False, "jemalloc", "mimalloc", "rpmalloc"], "enable_all": [True, False], "enable_libarchive": [True, False],
               "enable_rocksdb": [True, False], "enable_sftp": [True, False], "enable_prometheus": [True, False], "enable_bzip2": [True, False], "enable_lzma": [True, False],
               "enable_mqtt": [True, False], "enable_couchbase": [True, False], "enable_kafka": [True, False], "enable_opc": [True, False], "enable_gcp": [True, False],
               "enable_grpc_for_loki": [True, False], "enable_bustache": [True, False], "enable_kubernetes": [True, False], "enable_azure": [True, False], "enable_llamacpp": [True, False],
               "enable_aws": [True, False], "enable_sql": [True, False], "skip_tests": [True, False], "portable": [True, False]}

    default_options = {"shared": False, "fPIC": True, "custom_malloc": False, "enable_all": False, "enable_libarchive": False, "enable_rocksdb": False, "enable_sftp": False,
                       "enable_prometheus": False, "enable_bzip2": False, "enable_lzma": False, "enable_mqtt": False, "enable_couchbase": False, "enable_kafka": False, "enable_opc": False,
                       "enable_gcp": False, "enable_grpc_for_loki": False, "enable_bustache": False, "enable_kubernetes": False, "enable_azure": False, "enable_llamacpp": False, "enable_aws": False,
                       "enable_sql": False, "skip_tests": False, "portable": True}

    exports_sources = shared_sources

    def requirements(self):
        for req in shared_requires:
            self.requires(req)
        self.requires("lua/5.4.8", force=True)
        self.requires("asio/1.38.0", force=True)
        self.requires("lz4/1.10.0", force=True)
        self.requires("libcurl/8.20.0", force=True)
        self.requires("openssl/3.6.2", force=True)
        self.requires("zlib/1.3.2", force=True)
        self.requires("nlohmann_json/3.12.0", force=True)
        if self.settings.os != "Windows":
            self.requires("ossp-uuid/1.6.2@minifi/develop")
        if self.options.enable_libarchive:
            self.requires("libarchive/3.8.7")
        if self.options.enable_rocksdb:
            self.requires("rocksdb/11.1.1@minifi/develop")
        if self.options.enable_bzip2:
            self.requires("bzip2/1.0.8")
        if self.options.enable_lzma:
            self.requires("xz_utils/5.8.3")
        if self.options.enable_all or self.options.enable_sftp:
            self.requires("libssh2/1.11.1")
        if self.options.enable_all or self.options.enable_prometheus:
            self.requires("prometheus-cpp/1.3.0")
        if self.options.enable_all or self.options.enable_mqtt:
            self.requires("paho-mqtt-c/1.3.16@minifi/develop")
        if self.options.enable_all or self.options.enable_couchbase:
            self.requires("couchbase_cxx_client/1.3.1@minifi/develop")
            self.requires("ms-gsl/4.0.0")
            self.requires("snappy/1.2.1")
            self.requires("hdrhistogram-c/0.11.8")
            self.requires("taocpp-json/1.0.0-beta.14")
            self.requires("llhttp/9.3.0")
        if self.options.enable_all or self.options.enable_kafka:
            self.requires("librdkafka/2.14.2")
        if self.options.enable_all or self.options.enable_opc:
            self.requires("open62541/1.5.4@minifi/develop")
        if self.options.enable_all or self.options.enable_bustache:
            self.requires("bustache/0.1.0@minifi/develop")
        if self.options.enable_all or self.options.enable_grpc_for_loki:
            self.requires("grpc/1.82.0@minifi/develop", force=True)
        if self.options.enable_all or self.options.enable_gcp:
            self.requires("google-cloud-cpp/2.47.1@minifi/develop")
            if not self.options.skip_tests:
                self.requires("gtest/1.17.0")
        if (self.options.enable_all or self.options.enable_kubernetes) and self.settings.os != "Windows":
            self.requires("kubernetes/0.14.0@minifi/develop")
        if self.options.enable_all or self.options.enable_azure:
            self.requires("azure-sdk-for-cpp/12.18.0@minifi/develop")
        if self.options.enable_all or self.options.enable_llamacpp:
            self.requires("llama-cpp/b8944@minifi/develop")
        if self.options.enable_all or self.options.enable_aws:
            self.requires("aws-sdk-cpp/1.11.807@minifi/develop")
        if self.options.enable_all or self.options.enable_sql:
            self.requires("soci/4.1.4@minifi/develop")

        if self.options.custom_malloc == "jemalloc":
            self.requires("jemalloc/5.3.1")
        elif self.options.custom_malloc == "mimalloc":
            self.requires("mimalloc/3.3.2")
        elif self.options.custom_malloc == "rpmalloc":
            self.requires("rpmalloc/1.4.5@minifi/develop")

        if not self.options.skip_tests:
            self.requires("benchmark/1.9.5")
            self.requires("jolt-tests/0.1.8@minifi/develop")

    def build_requirements(self):
        if self.settings.os == "Windows":
            self.tool_requires("winflexbison/2.5.25")

    def configure(self):
        self.options["libarchive"].with_openssl = True
        self.options["date"].tz_db = "manual" if self.settings.os == "Windows" else "system"
        if self.options.enable_all or self.options.enable_bzip2:
            self.options["libarchive"].with_bzip2 = True
        if self.options.enable_all or self.options.enable_lzma:
            self.options["libarchive"].with_lzma = True
        if self.options.enable_all or self.options.enable_kafka:
            self.options["librdkafka"].ssl = True
            self.options["librdkafka"].sasl = False
            self.options["librdkafka"].zstd = True
            self.options["librdkafka"].zlib = True
        if (self.options.enable_all or self.options.enable_gcp) and not self.options.skip_tests:
            self.options["google-cloud-cpp"].with_mocks = True
        if self.options.enable_all or self.options.enable_aws:
            self.options["aws-sdk-cpp"].s3 = True
            self.options["aws-sdk-cpp"].kinesis = True
            setattr(self.options["aws-sdk-cpp"], "s3-crt", True)
            setattr(self.options["aws-sdk-cpp"], "text-to-speech", False)
        if self.options.enable_all or self.options.enable_llamacpp:
            self.options["llama-cpp"].portable = self.options.portable
        if self.options.enable_all or self.options.enable_mqtt:
            self.options["paho-mqtt-c"].high_performance = True
        if self.options.enable_all or self.options.enable_sql:
            self.options["soci"].shared = False
            self.options["soci"].with_odbc = True

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MINIFI_DEFAULT_DEPENDENCY_SOURCE"] = "CONAN"

        # Drop hardcoded install dir from Conan's generated toolchain to detect the correct install dir for RPM package installation
        tc.blocks.remove("output_dirs")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def overwrite_libfile(self, oldfile, newfile):
        print("Copying {} to {}".format(oldfile, newfile))
        if os.path.exists(oldfile):
            try:
                if os.path.exists(newfile):
                    os.remove(newfile)

                shutil.copy2(oldfile, newfile)
            except Exception as e:
                raise ConanException(f"Error copying {newfile}: {e}")
        else:
            raise ConanException(f"Error: {oldfile} does not exist")

    def package(self):
        cmake = CMake(self)
        cmake.install()
        include_dir = os.path.join(self.source_folder)
        built_dir = os.path.join(self.source_folder, self.folders.build)
        copy(self, pattern="*.h*", dst=os.path.join(self.package_folder, "include"), src=include_dir, keep_path=True)
        copy(self, pattern="*.i*", dst=os.path.join(self.package_folder, "include"), src=include_dir, keep_path=True)
        copy(self, pattern="*.a", dst=os.path.join(self.package_folder, "lib"), src=built_dir, keep_path=False)
        copy(self, pattern="*.so*", dst=os.path.join(self.package_folder, "lib"), src=built_dir, keep_path=False)

        minifi_py_ext_oldfile = os.path.join(self.package_folder, "lib", "libminifi-python-script-extension.so")
        minifi_py_ext_copynewfile = os.path.join(self.package_folder, "lib", "libminifi_native.so")
        self.overwrite_libfile(minifi_py_ext_oldfile, minifi_py_ext_copynewfile)

    def package_info(self):
        self.cpp_info.libs = collect_libs(self, folder=os.path.join(self.package_folder, "lib"))
        self.cpp_info.set_property("cmake_file_name", "minifi-cpp")
        self.cpp_info.set_property("cmake_target_name", "minifi-cpp::minifi-cpp")
        self.cpp_info.set_property("pkg_config_name", "minifi-cpp")

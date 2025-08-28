from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain
from conan.tools.files import collect_libs, copy
from conan.errors import ConanException
import os
import shutil

required_conan_version = ">=2.0"

shared_requires = ("openssl/3.2.1", "libcurl/8.4.0", "civetweb/1.16", "libxml2/2.12.6", "fmt/10.2.1", "spdlog/1.14.0", "catch2/3.5.4", "zlib/1.2.11", "zstd/1.5.2", "bzip2/1.0.8", "rocksdb/8.10.2@minifi/develop")

shared_sources = ("CMakeLists.txt", "libminifi/*", "extensions/*", "minifi_main/*", "nanofi/*", "bin/*", "bootstrap/*", "cmake/*", "conf/*", "controller/*", "encrypt-config/*", "etc/*", "examples/*", "packaging/msi/*", "thirdparty/*", "docker/*", "LICENSE", "NOTICE", "README.md", "C2.md", "CONFIGURE.md", "CONTRIBUTING.md", "CONTROLLERS.md", "EXPRESSIONS.md", "Extensions.md", "JNI.md", "METRICS.md", "OPS.md", "PROCESSORS.md", "ThirdParties.md", "Windows.md", "aptitude.sh", "arch.sh", "bootstrap.sh", "bstrp_functions.sh", "centos.sh", "CPPLINT.cfg", "darwin.sh", "debian.sh", "deploy.sh", "fedora.sh", "generateVersion.sh", "linux.sh", "rheldistro.sh", "run_clang_tidy.sh", "run_clang_tidy.sh", "run_flake8.sh", "run_shellcheck.sh", "suse.sh", "versioninfo.rc.in")


class MiNiFiCppMain(ConanFile):
    name = "minifi-cpp"
    version = "1.0.0"
    license = "Apache-2.0"
    requires = shared_requires
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = {"shared": [True, False], "fPIC": [True, False]}

    default_options = {"shared": False, "fPIC": True}

    exports_sources = shared_sources

    def generate(self):
        tc = CMakeToolchain(self)

        tc.variables["MINIFI_LIBCURL_SOURCE"] = "CONAN"
        tc.variables["MINIFI_OPENSSL_SOURCE"] = "CONAN"
        tc.variables["MINIFI_ZLIB_SOURCE"] = "CONAN"
        tc.variables["MINIFI_ROCKSDB_SOURCE"] = "CONAN"
        tc.variables["MINIFI_ZSTD_SOURCE"] = "CONAN"
        tc.variables["MINIFI_BZIP2_SOURCE"] = "CONAN"
        tc.variables["MINIFI_CIVETWEB_SOURCE"] = "CONAN"
        tc.variables["MINIFI_LIBXML2_SOURCE"] = "CONAN"
        tc.variables["MINIFI_FMT_SOURCE"] = "CONAN"
        tc.variables["MINIFI_SPDLOG_SOURCE"] = "CONAN"
        tc.variables["MINIFI_CATCH2_SOURCE"] = "CONAN"

        tc.variables["SKIP_TESTS"] = "OFF"
        tc.variables["ENABLE_CIVET"] = "ON"
        tc.variables["ENABLE_EXPRESSION_LANGUAGE"] = "ON"
        tc.variables["ENABLE_LIBARCHIVE"] = "OFF"
        tc.variables["ENABLE_AWS"] = "OFF"
        tc.variables["ENABLE_SQL"] = "OFF"
        tc.variables["ENABLE_GCP"] = "OFF"
        tc.variables["ENABLE_LUA_SCRIPTING"] = "OFF"

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

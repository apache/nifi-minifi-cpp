from conan import ConanFile
from conan.tools.env import VirtualRunEnv
from conan.tools.cmake import CMake, CMakeToolchain

required_conan_version = ">=2.1.0"

shared_requires = ("openssl/3.2.1", "libcurl/8.6.0")

class MiNiFiCppMain(ConanFile):
    name = "minifi-cpp-main"
    version = "0.99.0"
    license = "Apache-2.0"
    requires = shared_requires
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = {"shared": [True, False], "fPIC": [True, False]}

    default_options = {"shared": False, "fPIC": True,}

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["MINIFI_LIBCURL_SOURCE"] = "CONAN"
        tc.variables["MINIFI_OPENSSL_SOURCE"] = "CONAN"
        tc.variables["MINIFI_ZLIB_SOURCE"] = "CONAN"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

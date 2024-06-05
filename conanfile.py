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
        tc.variables["USE_CONAN_PACKAGER"] = "ON"

        tc.variables["ENABLE_OPENWSMAN"] = "OFF"
        tc.variables["ENABLE_CIVET"] = "OFF"
        tc.variables["SKIP_TESTS"] = "ON"
        tc.variables["ENABLE_EXPRESSION_LANGUAGE"] = "OFF"
        tc.variables["ENABLE_BZIP2"] = "OFF"
        tc.variables["ENABLE_ROCKSDB"] = "OFF"
        tc.variables["BUILD_ROCKSDB"] = "OFF"

        if self.settings.os == "Windows":
            tc.variables["ENABLE_WEL"] = "OFF"
            tc.variables["ENABLE_PDH"] = "OFF"
            tc.variables["ENABLE_SMB"] = "OFF"
        elif self.settings.os == "Linux":
            tc.variables["ENABLE_SYSTEMD"] = "OFF"
            tc.variables["ENABLE_PROCFS"] = "OFF"

        tc.variables["ENABLE_LIBARCHIVE"] = "OFF"
        tc.variables["ENABLE_LZMA"] = "OFF"
        tc.variables["ENABLE_GPS"] = "OFF"
        tc.variables["ENABLE_COAP"] = "OFF"
        tc.variables["ENABLE_SQL"] = "OFF"
        tc.variables["ENABLE_MQTT"] = "OFF"
        tc.variables["ENABLE_PCAP"] = "OFF"
        tc.variables["ENABLE_LIBRDKAFKA"] = "OFF"
        tc.variables["ENABLE_LUA_SCRIPTING"] = "OFF"
        tc.variables["ENABLE_PYTHON_SCRIPTING"] = "OFF"
        tc.variables["ENABLE_SENSORS"] = "OFF"
        tc.variables["ENABLE_USB_CAMERA"] = "OFF"
        tc.variables["ENABLE_AWS"] = "OFF"
        tc.variables["ENABLE_OPENCV"] = "OFF"
        tc.variables["ENABLE_BUSTACHE"] = "OFF"
        tc.variables["ENABLE_SFTP"] = "OFF"
        tc.variables["ENABLE_AZURE"] = "OFF"
        tc.variables["ENABLE_ENCRYPT_CONFIG"] = "OFF"
        tc.variables["ENABLE_SPLUNK"] = "OFF"
        tc.variables["ENABLE_ELASTICSEARCH"] = "OFF"
        tc.variables["ENABLE_GCP"] = "OFF"
        tc.variables["ENABLE_KUBERNETES"] = "OFF"
        tc.variables["ENABLE_TEST_PROCESSORS"] = "OFF"
        tc.variables["ENABLE_PROMETHEUS"] = "OFF"
        tc.variables["ENABLE_GRAFANA_LOKI"] = "OFF"
        tc.variables["ENABLE_GRPC_FOR_LOKI"] = "OFF"
        tc.variables["ENABLE_CONTROLLER"] = "OFF"

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class MiNiFiCppTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)
        self.requires("gsl-lite/0.41.0")
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.14.0")
        self.requires("catch2/3.5.4")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def layout(self):
        cmake_layout(self)

    def test(self):
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindirs[0], "abstract_processor_test")
            self.run(cmd, env="conanrun")

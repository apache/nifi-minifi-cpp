# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Recipe based on https://github.com/conan-io/conan-center-index/blob/master/recipes/llama-cpp/all/conanfile.py

import os
import textwrap

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os
from conan.tools.build import check_min_cppstd, cross_building
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import save, copy, get, rmdir, export_conandata_patches, apply_conandata_patches

required_conan_version = ">=2.0.9"


class LlamaCppConan(ConanFile):
    name = "llama-cpp"
    description = "Inference of LLaMA model in pure C/C++"
    topics = ("llama", "llm", "ai")
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/ggerganov/llama.cpp"
    license = "MIT"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "library"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_examples": [True, False],
        "with_cuda": [True, False],
        "with_curl": [True, False],
        "with_vulkan": [True, False],
        "portable": [True, False]
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_examples": False,
        "with_cuda": False,
        "with_curl": False,
        "with_vulkan": False,
        "portable": True
    }

    implements = ["auto_shared_fpic"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
        if is_apple_os(self):
            del self.options.with_vulkan

    @property
    def _cuda_build_module(self):
        # Adding this to the package info is necessary if we want consumers of llama to link correctly when
        # they activate the CUDA option. In the future, when we have a CUDA recipe this could be removed.
        cuda_target = "ggml-cuda"
        return textwrap.dedent(f"""\
            find_dependency(CUDAToolkit REQUIRED)
            if (WIN32)
                # As of CUDA 12.3.1, Windows does not offer a static cublas library
                target_link_libraries({cuda_target} INTERFACE CUDA::cudart_static CUDA::cublas CUDA::cublasLt CUDA::cuda_driver)
            else ()
                target_link_libraries({cuda_target} INTERFACE CUDA::cudart_static CUDA::cublas_static CUDA::cublasLt_static CUDA::cuda_driver)
            endif()
        """)

    def validate(self):
        check_min_cppstd(self, 17)

    def validate_build(self):
        if self.settings.compiler == "msvc" and "arm" in self.settings.arch:
            raise ConanInvalidConfiguration("llama-cpp does not support ARM architecture on msvc, it recommends to use clang instead")

    def export_sources(self):
        export_conandata_patches(self)

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        if self.options.with_curl:
            self.requires("libcurl/8.20.0")

        if self.options.get_safe("with_vulkan"):
            self.requires("vulkan-loader/[>=1.3 <1.5]")

    def build_requirements(self):
        if self.options.get_safe("with_vulkan"):
            self.tool_requires("shaderc/[>=2025.3]")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        apply_conandata_patches(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        if self.settings.compiler == "msvc":
            # llama-chat.cpp's LU8 macro relies on __cplusplus to cast u8 literals to (const char*),
            # but MSVC only reports the real __cplusplus value with /Zc:__cplusplus. Without it, the
            # literals stay char8_t under /std:c++latest and fail to compile.
            tc.extra_cxxflags.append("/Zc:__cplusplus")
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.variables["LLAMA_STANDALONE"] = False
        tc.variables["LLAMA_BUILD_TESTS"] = False
        tc.variables["LLAMA_BUILD_EXAMPLES"] = self.options.get_safe("with_examples")
        tc.variables["LLAMA_CURL"] = self.options.get_safe("with_curl")
        tc.variables["LLAMA_BUILD_SERVER"] = False
        tc.variables["LLAMA_BUILD_TOOLS"] = False
        tc.variables["LLAMA_BUILD_COMMON"] = True
        tc.variables["GGML_OPENMP"] = False
        tc.variables["GGML_METAL"] = False
        tc.variables["GGML_BLAS"] = False
        if cross_building(self):
            tc.variables["LLAMA_NATIVE"] = False
            tc.variables["GGML_NATIVE_DEFAULT"] = False

        tc.variables["GGML_BUILD_TESTS"] = False
        tc.variables["GGML_BUILD_EXAMPLES"] = False
        tc.variables["GGML_CUDA"] = self.options.get_safe("with_cuda")

        if self.options.get_safe("with_vulkan"):
            tc.variables["GGML_VULKAN"] = True
            shaderc_bin_path = os.path.join(self.dependencies.build["shaderc"].cpp_info.bindir, "glslc").replace("\\", "/")
            tc.variables["Vulkan_GLSLC_EXECUTABLE"] = shaderc_bin_path

        if self.options.portable:
            tc.variables["GGML_NATIVE"] = False
        else:
            tc.variables["GGML_NATIVE"] = True

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        copy(self, "*", os.path.join(self.source_folder, "models"), os.path.join(self.package_folder, "res", "models"))
        copy(self, "*.h*", os.path.join(self.source_folder, "common"), os.path.join(self.package_folder, "include", "common"))
        copy(self, "*.h*", os.path.join(self.source_folder, "tools", "mtmd"), os.path.join(self.package_folder, "include", "mtmd"))
        copy(self, "*mtmd*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*mtmd*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
        copy(self, "*mtmd*.so", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*mtmd*.dylib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*mtmd*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*common*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*common*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"), keep_path=False)
        copy(self, "*common*.so", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*common*.dylib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*common*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*httplib*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*httplib*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        if self.options.with_cuda and not self.options.shared:
            save(self, os.path.join(self.package_folder, "lib", "cmake", "llama-cpp-cuda-static.cmake"), self._cuda_build_module)

    def _get_backends(self):
        results = ["cpu"]
        if self.options.with_cuda:
            results.append("cuda")
        if self.options.get_safe("with_vulkan"):
            results.append("vulkan")
        return results

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "llamacpp")
        self.cpp_info.components["ggml"].libs = ["ggml"]
        self.cpp_info.components["ggml"].resdirs = ["res"]
        self.cpp_info.components["ggml"].set_property("cmake_target_name", "ggml::all")
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.components["ggml"].system_libs.append("dl")

        self.cpp_info.components["llama"].libs = ["llama"]
        self.cpp_info.components["llama"].resdirs = ["res"]
        self.cpp_info.components["llama"].requires.append("ggml")

        self.cpp_info.components["common"].includedirs = [os.path.join("include", "common")]
        if self.options.shared:
            self.cpp_info.components["common"].libs = ["llama-common"]
        else:
            self.cpp_info.components["common"].libs = ["llama-common", "llama-common-base", "cpp-httplib"]
        self.cpp_info.components["common"].requires = ["llama"]

        self.cpp_info.components["mtmd"].libs = ["mtmd"]
        self.cpp_info.components["mtmd"].includedirs = ["include"]
        self.cpp_info.components["mtmd"].requires = ["llama", "common"]

        if self.options.with_curl:
            self.cpp_info.components["common"].requires.append("libcurl::libcurl")
            self.cpp_info.components["common"].defines.append("LLAMA_USE_CURL")

        if is_apple_os(self):
            self.cpp_info.components["common"].frameworks.extend(["Foundation", "Accelerate", "Metal"])
        elif self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.components["common"].system_libs.extend(["dl", "m", "pthread", "gomp"])

        if self.options.with_cuda and not self.options.shared:
            self.cpp_info.builddirs.append(os.path.join("lib", "cmake"))
            module_path = os.path.join("lib", "cmake", "llama-cpp-cuda-static.cmake")
            self.cpp_info.set_property("cmake_build_modules", [module_path])

        self.cpp_info.components["ggml-base"].libs = ["ggml-base"]
        self.cpp_info.components["ggml-base"].resdirs = ["res"]
        self.cpp_info.components["ggml-base"].set_property("cmake_target_name", "ggml-base")

        self.cpp_info.components["ggml"].requires = ["ggml-base"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.components["ggml-base"].system_libs.extend(["dl", "m", "pthread"])


        if self.options.shared:
            self.cpp_info.components["llama"].defines.append("LLAMA_SHARED")
            self.cpp_info.components["ggml-base"].defines.append("GGML_SHARED")
            self.cpp_info.components["ggml"].defines.append("GGML_SHARED")

        backends = self._get_backends()
        for backend in backends:
            self.cpp_info.components[f"ggml-{backend}"].libs = [f"ggml-{backend}"]
            self.cpp_info.components[f"ggml-{backend}"].resdirs = ["res"]
            self.cpp_info.components[f"ggml-{backend}"].set_property("cmake_target_name", f"ggml-{backend}")
            if self.options.shared:
                self.cpp_info.components[f"ggml-{backend}"].defines.append("GGML_BACKEND_SHARED")
            self.cpp_info.components["ggml"].defines.append(f"GGML_USE_{backend.upper()}")
            self.cpp_info.components["ggml"].requires.append(f"ggml-{backend}")

            if backend == "vulkan":
                self.cpp_info.components["ggml-vulkan"].requires.append("vulkan-loader::vulkan-loader")

        if is_apple_os(self):
            if "blas" in backends:
                self.cpp_info.components["ggml-blas"].frameworks.append("Accelerate")
            if "metal" in backends:
                self.cpp_info.components["ggml-metal"].frameworks.extend(["Metal", "MetalKit", "Foundation", "CoreFoundation"])

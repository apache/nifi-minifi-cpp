import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualBuildEnv
from conan.tools.gnu import Autotools, AutotoolsToolchain
from conan.tools.layout import basic_layout
from conan.tools.files import apply_conandata_patches, collect_libs, copy, chdir, export_conandata_patches, get, rm, rmdir, save
from conan.tools.scm import Version
import os
import textwrap

required_conan_version = ">=1.53.0"

# TODO (JG): Add support for creating LibCoap 4.2.1 conan package for windows using minifi's libcoap-windows-cmake.patch
class LibCoapConan(ConanFile):
    name = "libcoap"
    description = "A CoAP (RFC 7252) implementation in C"
    license = "BSD-2-Clause"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/obgm/libcoap"
    topics = "coap"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_epoll": [True, False],
        "dtls_backend": [None, "openssl", "gnutls", "tinydtls", "mbedtls"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_epoll": False,
        "dtls_backend": "openssl",
    }

    # def export_sources(self):
    #     if self.settings.os == "Windows":
    #         export_conandata_patches(self)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def layout(self):
        basic_layout(self, src_folder="src")

    def requirements(self):
        if self.options.dtls_backend == "openssl":
            # self.requires("openssl/[>=1.1 <4]")
            self.requires("openssl/3.3.0@minifi/dev")
        elif self.options.dtls_backend == "mbedtls":
            # self.requires("mbedtls/3.2.1")
            self.requires("mbedtls/2.16.3@minifi/dev")

    def validate(self):
        if self.settings.os == "Windows" or is_apple_os(self):
            raise ConanInvalidConfiguration("Platform is currently not supported")

        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, 11)
        if self.options.dtls_backend in ["gnutls", "tinydtls"]:
            raise ConanInvalidConfiguration(f"{self.options.dtls_backend} not available yet")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        if self.settings.os == "Linux":
            env = VirtualBuildEnv(self)
            env.generate()
            tc = AutotoolsToolchain(self)
            tc.configure_args.append("--with-pic")
            tc.configure_args.append("--disable-examples")
            tc.configure_args.append("--disable-dtls")
            tc.configure_args.append("--disable-tests")
            tc.configure_args.append("--disable-documentation")
            tc.generate()

    def build(self):
        # if self.settings.os == "Windows":
        #     apply_conandata_patches(self)
        with chdir(self, self.source_folder):
            self.run("./autogen.sh")
            
        autotools = Autotools(self)
        autotools.configure()
        autotools.make()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        autotools = Autotools(self)
        autotools.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))
        rm(self, "*.la", os.path.join(self.package_folder, "lib"))

        self._create_cmake_module_variables(
            os.path.join(self.package_folder, self._module_file_rel_path),
        )

    def _create_cmake_module_variables(self, module_file):
        content = textwrap.dedent(f"""\
            set(COAP_FOUND TRUE)
            if(DEFINED libcoap_INCLUDE_DIRS)
                set(COAP_INCLUDE_DIRS ${{libcoap_INCLUDE_DIRS}})
            endif()
            if(DEFINED libcoap_LIBRARIES)
                set(COAP_LIBRARIES ${{libcoap_LIBRARIES}})
            endif()
            set(COAP_VERSION_MAJOR {Version(self.version).major})
            set(COAP_VERSION_MINOR {Version(self.version).minor})
            set(COAP_VERSION_PATCH {Version(self.version).patch})
            set(COAP_VERSION_STRING "{self.version}")
        """)
        save(self, module_file, content)

    @property
    def _module_file_rel_path(self):
        return os.path.join("lib", "cmake", f"conan-official-{self.name}-variables.cmake")

    def package_info(self):
        library_name = "coap-2"
        pkgconfig_filename = "libcoap-2"
        cmake_target_name = "libcoap"

        if self.options.dtls_backend:
            pkgconfig_filename += f"-{self.options.dtls_backend}"

        self.cpp_info.set_property("cmake_file_name", "libcoap")
        self.cpp_info.set_property("cmake_target_name", f"COAP::{cmake_target_name}")
        self.cpp_info.set_property("cmake_build_modules", [self._module_file_rel_path])

        self.cpp_info.set_property("pkg_config_name", pkgconfig_filename)

        # TODO: back to global scope once legacy generators support removed
        self.cpp_info.components["coap"].libs = [library_name]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["coap"].system_libs = ["pthread"]
            if self.options.dtls_backend == "openssl":
                self.cpp_info.components["coap"].requires = ["openssl::openssl"]
            elif self.options.dtls_backend == "mbedtls":
                self.cpp_info.components["coap"].requires = ["mbedtls::mbedtls"]

        # TODO: to remove once legacy generators support removed
        self.cpp_info.components["coap"].names["cmake_find_package"] = cmake_target_name
        self.cpp_info.components["coap"].names["cmake_find_package_multi"] = cmake_target_name
        self.cpp_info.components["coap"].set_property("cmake_target_name", f"COAP::{cmake_target_name}")
        self.cpp_info.components["coap"].set_property("pkg_config_name", pkgconfig_filename)
        self.cpp_info.build_modules["cmake_find_package"] = [self._module_file_rel_path]

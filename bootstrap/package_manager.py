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
import os
import platform
import subprocess
import sys
from typing import Dict, Set

from distro import distro


def _query_yes_no(question: str, no_confirm: bool) -> bool:
    valid = {"yes": True, "y": True, "ye": True, "no": False, "n": False}

    if no_confirm:
        print("Running {} with noconfirm".format(question))
        return True
    while True:
        print("{} [y/n]".format(question))
        choice = input().lower()
        if choice in valid:
            return valid[choice]
        else:
            print("Please respond with 'yes' or 'no' " "(or 'y' or 'n').")


def _run_command_with_confirm(command: str, no_confirm: bool) -> bool:
    if _query_yes_no("Running {}".format(command), no_confirm):
        return os.system(command) == 0


class PackageManager(object):
    def __init__(self, no_confirm):
        self.no_confirm = no_confirm
        pass

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        raise Exception("NotImplementedException")

    def install_compiler(self) -> str:
        raise Exception("NotImplementedException")

    def _install(self, dependencies: Dict[str, Set[str]], replace_dict: Dict[str, Set[str]], install_cmd: str) -> bool:
        dependencies.update({k: v for k, v in replace_dict.items() if k in dependencies})
        dependencies = self._filter_out_installed_packages(dependencies)
        dependencies_str = " ".join(str(value) for value_set in dependencies.values() for value in value_set)
        if not dependencies_str or dependencies_str.isspace():
            return True
        return _run_command_with_confirm(f"{install_cmd} {dependencies_str}", self.no_confirm)

    def _get_installed_packages(self) -> Set[str]:
        raise Exception("NotImplementedException")

    def _filter_out_installed_packages(self, dependencies: Dict[str, Set[str]]):
        installed_packages = self._get_installed_packages()
        filtered_packages = {k: (v - installed_packages) for k, v in dependencies.items()}
        for installed_package in installed_packages:
            filtered_packages.pop(installed_package, None)
        return filtered_packages

    def run_cmd(self, cmd: str) -> bool:
        result = subprocess.run(f"{cmd}", shell=True, text=True)
        return result.returncode == 0


class BrewPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="brew install",
                             replace_dict={"patch": set(),
                                           "jni": {"maven"}})

    def install_compiler(self) -> str:
        self.install({"compiler": set()})
        return ""

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['brew', 'list'], text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        lines = [line.split('@', 1)[0] for line in lines]
        return set(lines)


class AptPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="sudo apt install -y",
                             replace_dict={"libarchive": {"liblzma-dev"},
                                           "lua": {"liblua5.1-0-dev"},
                                           "python": {"libpython3-dev"},
                                           "libusb": {"libusb-1.0-0-dev", "libusb-dev"},
                                           "libpng": {"libpng-dev"},
                                           "libpcap": {"libpcap-dev"},
                                           "jni": {"openjdk-8-jdk", "openjdk-8-source", "maven"},
                                           "gpsd": {"libgps-dev"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['dpkg', '--get-selections'], text=True, capture_output=True, check=True)
        lines = [line.split('\t')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit(':', 1)[0] for line in lines]
        return set(lines)

    def install_compiler(self) -> str:
        if distro.id() == "ubuntu" and int(distro.major_version()) < 22:
            self.install({"compiler_prereq": {"apt-transport-https", "ca-certificates", "software-properties-common"}})
            _run_command_with_confirm("sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test",
                                      no_confirm=self.no_confirm)
            self.install({"compiler": {"build-essential", "g++-11"}})
            return "-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11"
        self.install({"compiler": {"g++"}})
        return ""


class DnfPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="sudo dnf --enablerepo=crb install -y epel-release",
                             replace_dict={"gpsd": {"gpsd-devel"},
                                           "libpcap": {"libpcap-devel"},
                                           "lua": {"lua-devel"},
                                           "python": {"python3-devel"},
                                           "jni": {"java-1.8.0-openjdk", "java-1.8.0-openjdk-devel", "maven"},
                                           "libpng": {"libpng-devel"},
                                           "libusb": {"libusb-devel"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['dnf', 'list', 'installed'], text=True, capture_output=True, check=True)
        lines = [line.split(' ')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit('.', 1)[0] for line in lines]
        return set(lines)

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc-c++"}})
        return ""


class PacmanPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="sudo pacman --noconfirm -S",
                             replace_dict={"jni": {"jdk8-openjdk", "maven"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['pacman', '-Qq'], text=True, capture_output=True, check=True)
        return set(result.stdout.splitlines())

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc"}})
        return ""


def _get_vs_dev_cmd_path() -> str:
    vswhere_results = subprocess.run(
        "vswhere -products * -property installationPath -requires Microsoft.VisualStudio.Component.VC.ATL",
        capture_output=True)

    for vswhere_result in vswhere_results.stdout.splitlines():
        possible_path = f"{vswhere_result.decode()}\\Common7\\Tools\\VsDevCmd.bat"
        if os.path.exists(possible_path):
            return f'"{possible_path}"'
    raise Exception("Could not find valid Visual Studio installation")


def _get_vs_dev_cmd() -> str:
    vs_dev_path = _get_vs_dev_cmd_path()
    return f"{vs_dev_path} -arch=x64 -host_arch=x64"


class ChocolateyPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        self._install(dependencies=dependencies,
                      install_cmd="choco install -y",
                      replace_dict={"lua": set(),
                                    "python": set(),
                                    "patch": set(),
                                    "bison": set(),
                                    "flex": set(),
                                    "libarchive": set(),
                                    "libpcap": set(),
                                    "libpng": set(),
                                    "gpsd": set(),
                                    "automake": set(),
                                    "autoconf": set(),
                                    "libtool": set(),
                                    "libusb": set(),
                                    "make": set(),
                                    "jni": {"openjdk", "maven"},
                                    "openssl": {"strawberryperl", "NASM"}})
        return True

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['choco', 'list'], text=True, capture_output=True, check=True)
        lines = [line.split(' ')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit('.', 1)[0] for line in lines]
        if os.path.exists("C:\\Program Files\\NASM"):
            lines.append("NASM")    # choco doesnt remember NASM
        return set(lines)

    def install_compiler(self) -> str:
        self.install({"visualstudio2022buildtools": {'visualstudio2022buildtools --package-parameters "--wait --quiet '
                                                     '--add Microsoft.VisualStudio.Workload.VCTools '
                                                     '--add Microsoft.VisualStudio.Component.VC.ATL '
                                                     '--includeRecommended"'},
                      "vswhere": {"vswhere"}})
        return ""

    def run_cmd(self, cmd: str) -> bool:
        cmd_command = f"refreshenv & {_get_vs_dev_cmd()} & set PATH=!PATH:C:\\Strawberry\\c\\bin;=!;C:\\Program Files\\NASM; & {cmd}"
        cmd_command_list = f'cmd /V:ON /C {cmd_command}'
        res = subprocess.run(cmd_command_list, check=True, text=True)

        return res.returncode == 0


def get_package_manager(no_confirm: bool) -> PackageManager:
    platform_system = platform.system()
    if platform_system == "Darwin":
        return BrewPackageManager(no_confirm)
    elif platform_system == "Linux":
        distro_id = distro.id()
        if distro_id == "ubuntu":
            return AptPackageManager(no_confirm)
        elif "arch" in distro_id or "manjaro" in distro_id:
            return PacmanPackageManager(no_confirm)
        elif "rocky" in distro_id:
            return DnfPackageManager(no_confirm)
        else:
            sys.exit(f"Unsupported platform {distro_id} exiting")
    elif platform_system == "Windows":
        return ChocolateyPackageManager(no_confirm)
    else:
        sys.exit(f"Unsupported platform {platform_system} exiting")

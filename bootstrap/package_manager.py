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
import pathlib
import platform
import subprocess
import sys
from enum import Enum
from typing import Dict, Set

from distro import distro


class VsWhereLocation(Enum):
    CHOCO = 1
    DEFAULT = 2


def _query_yes_no(question: str, no_confirm: bool) -> bool:
    valid = {"yes": True, "y": True, "ye": True, "no": False, "n": False}

    if no_confirm:
        print("Running {} with noconfirm".format(question))
        return True
    while True:
        print("{} [y/n]".format(question), end=' ', flush=True)
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

    def ensure_environment(self):
        pass

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
                             replace_dict={"patch": set()})

    def install_compiler(self) -> str:
        self.install({"compiler": set()})
        return ""

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['brew', 'list'], text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        lines = [line.split('@', 1)[0] for line in lines]
        return set(lines)

    def run_cmd(self, cmd: str) -> bool:
        add_m4_to_path_cmd = 'export PATH="$(brew --prefix m4)/bin:$PATH"'
        result = subprocess.run(f"{add_m4_to_path_cmd} && {cmd}", shell=True, text=True)
        return result.returncode == 0


class AptPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="sudo apt install -y",
                             replace_dict={"libarchive": {"liblzma-dev"},
                                           "python": {"libpython3-dev"},
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
    def __init__(self, no_confirm, needs_epel):
        PackageManager.__init__(self, no_confirm)
        self.needs_epel = needs_epel

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        if self.needs_epel:
            install_cmd = "sudo dnf --enablerepo=crb install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm"
        else:
            install_cmd = "sudo dnf install -y"
        return self._install(dependencies=dependencies,
                             install_cmd=install_cmd,
                             replace_dict={"gpsd": {"gpsd-devel"},
                                           "python": {"python3-devel"}})

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
                             replace_dict={})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['pacman', '-Qq'], text=True, capture_output=True, check=True)
        return set(result.stdout.splitlines())

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc"}})
        return ""


class ZypperPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        return self._install(dependencies=dependencies,
                             install_cmd="sudo zypper install -y",
                             replace_dict={"libarchive": {"libarchive-devel"},
                                           "python": {"python3-devel"}})

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['zypper', 'se', '--installed-only'], text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        packages = set()
        for line in lines:
            if line.startswith('S |') or line.startswith('--') or line.startswith(' ') or not line:
                continue

            parts = line.split('|')
            if len(parts) > 2:
                package_name = parts[1].strip()
                packages.add(package_name)

        return packages

    def install_compiler(self) -> str:
        self.install({"compiler": {"gcc-c++"}})
        return ""


def _get_vs_dev_cmd_path(vs_where_location: VsWhereLocation):
    if vs_where_location == VsWhereLocation.CHOCO:
        vs_where_path = "vswhere"
    else:
        vs_where_path = "%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe"

    vswhere_results = subprocess.run(
        f"{vs_where_path} -products * "
        f"-property installationPath "
        f"-requires Microsoft.VisualStudio.Component.VC.ATL "
        f"-version 17",
        capture_output=True)

    for vswhere_result in vswhere_results.stdout.splitlines():
        possible_path = f"{vswhere_result.decode()}\\Common7\\Tools\\VsDevCmd.bat"
        if os.path.exists(possible_path):
            return f'"{possible_path}"'
    return None


def _get_vs_dev_cmd(vs_where_location: VsWhereLocation) -> str:
    vs_dev_path = _get_vs_dev_cmd_path(vs_where_location)
    return f"{vs_dev_path} -arch=x64 -host_arch=x64"


def _get_activate_venv_path():
    return pathlib.Path(__file__).parent.resolve() / "venv" / "Scripts" / "activate.bat"


def _minifi_setup_env_str(vs_where_location: VsWhereLocation) -> str:
    return f"""
call refreshenv
call {_get_vs_dev_cmd(vs_where_location)}
setlocal EnableDelayedExpansion
  set PATH=!PATH:C:\\Strawberry\\c\\bin;=!;C:\\Program Files\\NASM;
endlocal & set PATH=%PATH%
set build_platform=x64
IF "%VIRTUALENV%"=="" (
  echo already in venv
) ELSE (
  {_get_activate_venv_path()}
)

"""


def _create_minifi_setup_env_batch(vs_where_location: VsWhereLocation):
    with open(pathlib.Path(__file__).parent.resolve() / "build_environment.bat", "w") as f:
        f.write(_minifi_setup_env_str(vs_where_location))


class ChocolateyPackageManager(PackageManager):
    def __init__(self, no_confirm):
        PackageManager.__init__(self, no_confirm)

    def install(self, dependencies: Dict[str, Set[str]]) -> bool:
        self._install(dependencies=dependencies,
                      install_cmd="choco install -y",
                      replace_dict={"python": set(),
                                    "patch": set(),
                                    "bison": set(),
                                    "flex": set(),
                                    "libarchive": set(),
                                    "gpsd": set(),
                                    "automake": set(),
                                    "autoconf": set(),
                                    "libtool": set(),
                                    "make": set(),
                                    "perl": {"strawberryperl", "NASM"}})
        return True

    def _get_installed_packages(self) -> Set[str]:
        result = subprocess.run(['choco', 'list'], text=True, capture_output=True, check=True)
        lines = [line.split(' ')[0] for line in result.stdout.splitlines()]
        lines = [line.rsplit('.', 1)[0] for line in lines]
        if os.path.exists("C:\\Program Files\\NASM"):
            lines.append("NASM")  # choco doesnt remember NASM
        return set(lines)

    def _acquire_vswhere(self):
        installed_packages = self._get_installed_packages()
        if "vswhere" in installed_packages:
            return VsWhereLocation.CHOCO
        vswhere_default_path = "%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe"
        if os.path.exists(vswhere_default_path):
            return VsWhereLocation.DEFAULT
        self.install({"vswhere": {"vswhere"}})
        return VsWhereLocation.CHOCO

    def install_compiler(self) -> str:
        vs_where_loc = self._acquire_vswhere()
        vs_dev_path = _get_vs_dev_cmd_path(vs_where_loc)
        if not vs_dev_path:
            self.install(
                {"visualstudio2022buildtools": {'visualstudio2022buildtools --package-parameters "--wait --quiet '
                                                '--add Microsoft.VisualStudio.Workload.VCTools '
                                                '--add Microsoft.VisualStudio.Component.VC.ATL '
                                                '--includeRecommended"'}})
        return ""

    def run_cmd(self, cmd: str) -> bool:
        env_bat_path = pathlib.Path(__file__).parent.resolve() / "build_environment.bat"
        res = subprocess.run(f"{env_bat_path} & {cmd}", check=True, text=True)

        return res.returncode == 0

    def ensure_environment(self):
        _create_minifi_setup_env_batch(self._acquire_vswhere())


def get_package_manager(no_confirm: bool) -> PackageManager:
    platform_system = platform.system()
    if platform_system == "Darwin":
        return BrewPackageManager(no_confirm)
    elif platform_system == "Linux":
        distro_id = distro.id()
        if distro_id == "ubuntu" or "debian" in distro_id:
            return AptPackageManager(no_confirm)
        elif "arch" in distro_id or "manjaro" in distro_id:
            return PacmanPackageManager(no_confirm)
        elif "rocky" in distro_id:
            return DnfPackageManager(no_confirm, True)
        elif "fedora" in distro_id:
            return DnfPackageManager(no_confirm, False)
        elif "opensuse" in distro_id:
            return ZypperPackageManager(no_confirm)
        else:
            sys.exit(f"Unsupported platform {distro_id} exiting")
    elif platform_system == "Windows":
        return ChocolateyPackageManager(no_confirm)
    else:
        sys.exit(f"Unsupported platform {platform_system} exiting")

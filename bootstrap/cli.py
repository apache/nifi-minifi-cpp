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
import inquirer
import yaml
from pathlib import Path

from minifi_option import MinifiOptions
from package_manager import PackageManager
from system_dependency import install_required


def install_dependencies(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    res = install_required(minifi_options, package_manager)
    print("Installation went smoothly" if res else "There were some error during installation")
    return res


def export_custom_conan_recipes(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    thirdparty_dir = minifi_options.source_dir / "thirdparty"
    for root, _, files in os.walk(thirdparty_dir):
        for file in files:
            if file == "conanfile.py":
                config_yaml = os.path.join(Path(root).parent, "config.yml")

                with open(config_yaml) as f:
                    data = yaml.safe_load(f)

                version = next(iter(data["versions"]))

                print(f"Exporting the custom Conan recipe {root} with version {version}")
                if not package_manager.run_cmd(f"conan export {root} --version={version} --user=minifi --channel=develop"):
                    print(f"Exporting the custom Conan recipe {root} failed")
                    return False
    return True


def add_conan_options_from_cmake_options(extension_options: list[str], minifi_options: MinifiOptions) -> str:
    conan_options = ""
    for extension_option in extension_options:
        if minifi_options.bool_options[extension_option.upper()].value not in (None, "OFF"):
            conan_options += f' -o "&:{extension_option.lower()}=True"'
    return conan_options


def run_conan_install(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    if not minifi_options.use_conan.value == "ON":
        print("Conan install skipped because USE_CONAN is OFF")
        return True
    conan_options = add_conan_options_from_cmake_options(["ENABLE_ALL", "ENABLE_LIBARCHIVE", "ENABLE_ROCKSDB", "ENABLE_SFTP", "ENABLE_PROMETHEUS", "ENABLE_BZIP2", "ENABLE_LZMA", "ENABLE_MQTT",
                                                          "ENABLE_COUCHBASE", "ENABLE_KAFKA", "ENABLE_OPC", "SKIP_TESTS"], minifi_options)
    if minifi_options.custom_malloc is not None and minifi_options.custom_malloc.value not in (None, "OFF"):
        conan_options += f' -o "&:custom_malloc={minifi_options.custom_malloc.value}"'

    if not package_manager.run_cmd("conan profile detect --exist-ok"):
        print("Conan default profile detection failed")
        return False

    if not export_custom_conan_recipes(minifi_options, package_manager):
        return False

    compiler_settings = " --settings=compiler.cppstd=23"
    generator_setting = " -c tools.cmake.cmaketoolchain:generator=Ninja" if minifi_options.use_ninja.value == "ON" else ""
    conan_remote_add_cmd = "conan remote add nifi-conan https://apache.jfrog.io/artifactory/api/conan/nifi-conan --force"
    if not package_manager.run_cmd(conan_remote_add_cmd):
        print("Adding the nifi-conan remote failed")
        return False
    build_cmd = f"conan install {minifi_options.source_dir} --output-folder={minifi_options.build_dir} --build=missing {conan_options} " \
                f"--settings=build_type={minifi_options.build_type.value}{generator_setting}{compiler_settings}"
    res = package_manager.run_cmd(build_cmd)
    print("Conan install was successful" if res else "Conan install was unsuccessful")
    return res


def _conan_build_env_prefix(minifi_options: MinifiOptions) -> str:
    if minifi_options.use_conan.value == "ON" and platform.system() == "Windows":
        conanbuild = os.path.join(str(minifi_options.build_dir), "conanbuild.bat")
        return f'call "{conanbuild}" && '
    return ""


def run_cmake(minifi_options: MinifiOptions, package_manager: PackageManager):
    if not os.path.exists(minifi_options.build_dir):
        os.mkdir(minifi_options.build_dir)
    cmake_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake {minifi_options.create_cmake_generator_str()} {minifi_options.create_cmake_use_conan_str()} " \
                f"{minifi_options.create_cmake_options_str()} {minifi_options.source_dir} -B {minifi_options.build_dir}"
    res = package_manager.run_cmd(cmake_cmd)
    print("CMake command run successfully" if res else "CMake command run unsuccessfully")
    return res


def do_build(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake --build {str(minifi_options.build_dir)} {minifi_options.create_cmake_build_flags_str()}"
    res = package_manager.run_cmd(build_cmd)
    print("Build was successful" if res else "Build was unsuccessful")
    return res


def do_package(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"{_conan_build_env_prefix(minifi_options)}cmake --build {str(minifi_options.build_dir)} --target package {minifi_options.create_cmake_build_flags_str()}"
    return package_manager.run_cmd(build_cmd)


def do_docker_build(minifi_options: MinifiOptions, package_manager: PackageManager):
    build_cmd = f"cmake --build {str(minifi_options.build_dir)} --target docker"
    return package_manager.run_cmd(build_cmd)


def do_one_click_build(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    assert install_dependencies(minifi_options, package_manager)
    assert run_conan_install(minifi_options, package_manager)
    assert run_cmake(minifi_options, package_manager)
    assert do_build(minifi_options, package_manager)
    assert do_package(minifi_options, package_manager)
    return True


def do_one_click_configuration(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    assert install_dependencies(minifi_options, package_manager)
    assert run_conan_install(minifi_options, package_manager)
    assert run_cmake(minifi_options, package_manager)
    return True


def main_menu(minifi_options: MinifiOptions, package_manager: PackageManager):
    done = False
    while not done:
        main_menu_options = {
            # All menu options' functions return True if the bootstrap should exit after execution
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            f"Build type: {minifi_options.build_type.value}": build_type_menu,
            f"Custom malloc: {minifi_options.custom_malloc.value if minifi_options.custom_malloc is not None else 'N/A'}": custom_malloc_menu,
            "Build options": build_options_menu,
            "Extension options": extension_options_menu,
            "One click build": do_one_click_build,
            "Step by step build": step_by_step_menu,
            "Exit": lambda _options, _manager: True,
        }

        questions = [
            inquirer.List(
                "sub_menu",
                message="Main Menu",
                choices=[menu_option_name for menu_option_name in main_menu_options],
            ),
        ]

        main_menu_prompt = inquirer.prompt(questions)
        if main_menu_prompt is None:
            break
        done = main_menu_options[main_menu_prompt["sub_menu"]](minifi_options, package_manager)


def build_type_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    questions = [
        inquirer.List(
            "build_type",
            message="Build type",
            choices=minifi_options.build_type.possible_values,
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.build_type.value = answers["build_type"]
    minifi_options.save_option_state()
    return False


def custom_malloc_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    if minifi_options.custom_malloc is None:
        return False
    questions = [
        inquirer.List(
            "custom_malloc",
            message="Custom malloc implementation (only jemalloc and mimalloc are provided via Conan)",
            choices=minifi_options.custom_malloc.possible_values,
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.custom_malloc.value = answers["custom_malloc"]
    minifi_options.save_option_state()
    return False


def build_dir_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    questions = [
        inquirer.Path('build_dir',
                      message="Build directory",
                      default=minifi_options.build_dir
                      ),
    ]
    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    minifi_options.build_dir = answers["build_dir"]
    minifi_options.save_option_state()
    return False


def extension_options_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    possible_values = [option_name for option_name in minifi_options.extension_options]
    selected_values = [option.name for option in minifi_options.extension_options.values() if option.value == "ON"]
    questions = [
        inquirer.Checkbox(
            "options",
            message="MiNiFi C++ Extension Options (space to select, enter to confirm)",
            choices=possible_values,
            default=selected_values
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    for extension_option in minifi_options.extension_options.values():
        if extension_option.name in answers["options"]:
            extension_option.value = "ON"
        else:
            extension_option.value = "OFF"

    minifi_options.save_option_state()
    return False


def build_options_menu(minifi_options: MinifiOptions, _package_manager: PackageManager) -> bool:
    possible_values = [option_name for option_name in minifi_options.build_options]
    selected_values = [option.name for option in minifi_options.build_options.values() if option.value == "ON"]
    questions = [
        inquirer.Checkbox(
            "options",
            message="MiNiFi C++ Build Options (space to select, enter to confirm)",
            choices=possible_values,
            default=selected_values
        ),
    ]

    answers = inquirer.prompt(questions)
    if answers is None:
        return True
    for build_option in minifi_options.build_options.values():
        if build_option.name in answers["options"]:
            build_option.value = "ON"
        else:
            build_option.value = "OFF"

    minifi_options.save_option_state()
    return False


def step_by_step_menu(minifi_options: MinifiOptions, package_manager: PackageManager) -> bool:
    done = False
    while not done:
        step_by_step_options = {
            # All menu options' functions return True if the bootstrap should exit after execution
            f"Build dir: {minifi_options.build_dir}": build_dir_menu,
            "Install dependencies": install_dependencies,
            "Run conan install": run_conan_install,
            "Run cmake": run_cmake,
            "Build": do_build,
            "Package": do_package,
            "Docker build": do_docker_build,
            "Back": lambda _options, _manager: True,
        }
        questions = [
            inquirer.List(
                "selection",
                message="Step by step menu",
                choices=[step_by_step_menu_option_name for step_by_step_menu_option_name in step_by_step_options],
            ),
        ]

        step_by_step_prompt = inquirer.prompt(questions)
        if step_by_step_prompt is None:
            return True
        step_by_step_options[step_by_step_prompt["selection"]](minifi_options, package_manager)
        done = step_by_step_prompt['selection'] == 'Back'
    return False

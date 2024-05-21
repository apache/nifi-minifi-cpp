import os
import argparse

SCRIPT_PATH = os.path.realpath(__file__)
SCRIPT_DIR = os.path.dirname(SCRIPT_PATH)

def get_include_dirs(project_root_dir):
    include_dirs = []
    for root, dirs, files in os.walk(project_root_dir):
        include_dirs.append(root)
    return include_dirs

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate include_dirs.cmake file")
    parser.add_argument("project_root_dir", help="Path to the project root directory")
    parser.add_argument("include_dirs_cmake_file", help="Path to the output include_dirs.cmake file")
    parser.add_argument("include_dirs_variable_name", help="Name of the CMake variable to store include directories")
    args = parser.parse_args()

    root_dir = args.project_root_dir
    include_dirs = get_include_dirs(root_dir)

    gen_include_dirs_cmake = args.include_dirs_cmake_file
    with open(gen_include_dirs_cmake, "w") as f:
        f.write(f"set({args.include_dirs_variable_name}\n")
        for dir in include_dirs:
            f.write(f"    \"{dir}\"\n")
        f.write(")\n")

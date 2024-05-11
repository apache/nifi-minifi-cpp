import os
import argparse

SCRIPT_PATH = os.path.realpath(__file__)
SCRIPT_DIR = os.path.dirname(SCRIPT_PATH)

# go to ezbuild to get script_Dir
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

    # TODO (JG): Pass CMakeLists.txt current source path with include/ to root_dir
    root_dir = args.project_root_dir
    # root_dir = os.path.join(SCRIPT_DIR, "..", "..", "include")
    include_dirs = get_include_dirs(root_dir)

    # TODO (JG): From CMakeLists.txt, pass location of where we want .cmake to be stored in our case libminifi/cmake
    # TODO (JG): From CMakeLists.txt, pass variable name of INCLUDE_DIRS in our case LIBMINIFI_INCLUDE_DIRS
    # gen_include_dirs_cmake = os.path.join(SCRIPT_DIR, "..", "include_dirs.cmake")
    gen_include_dirs_cmake = args.include_dirs_cmake_file
    with open(gen_include_dirs_cmake, "w") as f:
        f.write(f"set({args.include_dirs_variable_name}\n")
        for dir in include_dirs:
            f.write(f"    \"{dir}\"\n")
        f.write(")\n")

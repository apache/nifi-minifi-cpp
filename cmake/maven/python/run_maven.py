import os
import argparse
import subprocess

SCRIPT_PATH = os.path.realpath(__file__)
SCRIPT_DIR = os.path.dirname(SCRIPT_PATH)

def run_maven(maven_executable, maven_arg1=None, maven_nargs=None):
    output = subprocess.check_output([maven_executable, maven_arg1], universal_newlines=True)

    print(output)

def run_maven_from_wk_dir(maven_executable, maven_nargs=None, working_dir=None):
    if maven_nargs is not None and working_dir is not None:
        print(f"Running Maven to build Java SFTP Test Server")
        orig_dir = os.getcwd()

        os.chdir(working_dir)

        maven_command = [maven_executable, *maven_nargs]
        output = subprocess.check_output(maven_command, universal_newlines=True)

        print(output)

        os.chdir(orig_dir)

if __name__ == "__main__":
    print("Running Python to Run Conan Maven")
    parser = argparse.ArgumentParser(description="Generate include_dirs.cmake file")
    parser.add_argument("maven_executable_path", help="Path to the maven executable")
    parser.add_argument("--maven_arg1", dest="maven_arg1", nargs="?", help="Maven 1st argument")
    parser.add_argument("--maven_arg2", dest="maven_arg2", nargs="?", help="Maven 2nd argument")
    parser.add_argument("--maven_arg3", dest="maven_arg3", nargs="?", help="Maven 3rd argument")
    parser.add_argument("--working_directory", dest="working_directory", nargs="?", help="Working directory to run maven from")
    args = parser.parse_args()

    maven_executable = args.maven_executable_path

    if args.maven_arg1 and args.maven_arg2 and args.maven_arg3 and args.working_directory:
        print(f"-----maven_nargs: {args.maven_arg1}; {args.maven_arg2}; {args.maven_arg3}")
        print(f"-----working_directory: {args.working_directory}")

        run_maven_from_wk_dir(maven_executable, maven_nargs=[args.maven_arg1, args.maven_arg2, args.maven_arg3], working_dir=args.working_directory)
    if args.maven_arg1 and args.maven_arg2 and args.working_directory:
        print(f"-----maven_nargs: {args.maven_arg1}; {args.maven_arg2}")
        print(f"-----working_directory: {args.working_directory}")

        run_maven_from_wk_dir(maven_executable, maven_nargs=[args.maven_arg1, args.maven_arg2], working_dir=args.working_directory)
    elif args.maven_arg1:
        print(f"-----maven_arg1: {args.maven_arg1}")
        run_maven(maven_executable, maven_arg1=args.maven_arg1)
    else:
        run_maven(maven_executable, "--version")

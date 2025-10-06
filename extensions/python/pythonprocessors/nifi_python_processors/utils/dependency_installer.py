import ast
import sys
import subprocess
import os


# Extract the list of PIP dependency packages from the visited processor class AST node
class Visitor(ast.NodeVisitor):
    def __init__(self, class_name):
        self.dependencies = []
        self.class_name = class_name

    def visit_ClassDef(self, node):
        if node.name != self.class_name:
            return

        # Iterate through the body of the class to find the ProcessorDetails nested class
        for child in node.body:
            if isinstance(child, ast.ClassDef) and child.name == 'ProcessorDetails':
                # Iterate through the nodes of the 'ProcessorDetails' class
                for detail in child.body:
                    # Check if the child node is an assignment of the 'dependencies' member variable
                    if isinstance(detail, ast.Assign) and detail.targets[0].id == 'dependencies':
                        # Iterate through values of the 'dependencies' list member variable
                        for elt in detail.value.elts:
                            # Check if the element is a string constant and add it to the dependencies list
                            if isinstance(elt, ast.Constant):
                                self.dependencies.append(elt.value)
                        break
                break


def extract_dependencies(file_path):
    class_name = file_path.split(os.sep)[-1].split('.')[0]
    with open(file_path, 'r') as file:
        code = file.read()

    tree = ast.parse(code)
    visitor = Visitor(class_name)
    visitor.visit(tree)
    return visitor.dependencies


def has_progress_bar() -> bool:
    return sys.version_info.major > 3 or (sys.version_info.major == 3 and sys.version_info.minor >= 9)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(1)

    print("Checking dependencies for MiNiFi python processors...")

    # --no-cache-dir is used to be in line with NiFi's dependency install behavior
    if has_progress_bar():
        command = [sys.executable, "-m", "pip", "install", "--no-cache-dir", "--progress-bar", "off"]
    else:
        command = [sys.executable, "-m", "pip", "install", "--no-cache-dir"]
    dependencies_found = False
    for i in range(1, len(sys.argv)):
        if "requirements.txt" in sys.argv[i]:
            command += ["-r", sys.argv[i]]
            print("Installing dependencies from requirements file: {}".format(sys.argv[i]))
            dependencies_found = True
        else:
            dependencies = extract_dependencies(sys.argv[i])
            if dependencies:
                dependencies_found = True
                print("Installing dependencies for processor {}: {}".format(sys.argv[i], str(dependencies)))
                command += dependencies

    if dependencies_found:
        try:
            subprocess.check_call(command)
            print("Done installing dependencies for MiNiFi python processors.")
        except subprocess.CalledProcessError as e:
            print("Error occurred while installing dependencies: {}".format(e))
            sys.exit(1)
    else:
        print("No dependencies were found for MiNiFi python processors. No python packages were installed.")

import ast
import sys
import subprocess
import os


class Visitor(ast.NodeVisitor):
    def __init__(self, class_name):
        self.dependencies = []
        self.class_name = class_name

    def visit_ClassDef(self, node):
        if node.name != self.class_name:
            return
        for child in node.body:
            if isinstance(child, ast.ClassDef) and child.name == 'ProcessorDetails':
                for detail in child.body:
                    if isinstance(detail, ast.Assign) and detail.targets[0].id == 'dependencies':
                        for elt in detail.value.elts:
                            if isinstance(elt, ast.Constant):
                                self.dependencies.append(elt.s)
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


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(1)

    dependencies = extract_dependencies(sys.argv[1])
    if dependencies:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--no-cache-dir"] + dependencies)

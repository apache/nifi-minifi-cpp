#include <iostream>

#define Py_LIMITED_API
#include <Python.h>

static struct PyModuleDef spammodule = {
    PyModuleDef_HEAD_INIT,
    "minifi_native",   /* name of module */
    nullptr, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    nullptr,
    nullptr
};

PyMODINIT_FUNC
PyInit_minifi_native(void)
{
    return PyModule_Create(&spammodule);
}

int main() {
    std::cout << "buzi" << std::endl;
    return 0;
}
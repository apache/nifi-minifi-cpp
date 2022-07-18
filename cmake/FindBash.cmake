# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindBash
---------

This is a modified version of FindPatch.cmake.

The module defines the following variables:

``Bash_EXECUTABLE``
  Path to the bash command-line executable.
``Bash_FOUND``
  True if the bash command-line executable was found.

The following :prop_tgt:`IMPORTED` targets are also defined:

``Bash::bash``
  The command-line executable.

Example usage:

.. code-block:: cmake

   find_package(Bash)
   if(Bash_FOUND)
     message("Bash found: ${Bash_EXECUTABLE}")
   endif()
#]=======================================================================]

set(_doc "Bash command line executable")

if(CMAKE_HOST_WIN32)
    # First search the directories under the user's AppData
    set(_bash_path
        "$ENV{LOCALAPPDATA}/Programs/Git/bin"
        "$ENV{LOCALAPPDATA}/Programs/Git/usr/bin"
        "$ENV{APPDATA}/Programs/Git/bin"
        "$ENV{APPDATA}/Programs/Git/usr/bin"
        )
    find_program(Bash_EXECUTABLE
        NAMES bash
        NO_DEFAULT_PATH
        PATHS ${_bash_path}
        DOC ${_doc}
        )

    # Now look for installations in Git/ directories under typical installation
    # prefixes on Windows.
    find_program(Bash_EXECUTABLE
        NAMES bash
        NO_SYSTEM_ENVIRONMENT_PATH
        PATH_SUFFIXES Git/usr/bin Git/bin GnuWin32/bin
        DOC ${_doc}
        )

    unset(_bash_path)
else()
    find_program(Bash_EXECUTABLE
        NAMES bash
        DOC ${_doc}
        )
endif()

if(Bash_EXECUTABLE AND NOT TARGET Bash::bash)
    add_executable(Bash::bash IMPORTED)
    set_property(TARGET Bash::bash PROPERTY IMPORTED_LOCATION ${Bash_EXECUTABLE})
endif()

unset(_doc)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Bash
                                  REQUIRED_VARS Bash_EXECUTABLE)

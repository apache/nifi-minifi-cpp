<!--
  Licensed to the Apache Software Foundation (ASF) under one or more
  contributor license agreements.  See the NOTICE file distributed with
  this work for additional information regarding copyright ownership.
  The ASF licenses this file to You under the Apache License, Version 2.0
  (the "License"); you may not use this file except in compliance with
  the License.  You may obtain a copy of the License at
      http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-->
# Apache MiNiFi C++ Third Parties guide

Apache MiNiFi C++ uses many third party libraries, both for core functionality and for extensions.

This document describes the way we build and use third parties and provides a guide for adding new ones.

## Table of Contents

  * [Table of Contents](#table-of-contents)
  * [Choosing a third party](#choosing-a-third-party)
    + [License](#license)
  * [Built-in or system dependency](#built-in-or-system-dependency)
  * [System dependency](#system-dependency)
    + [bootstrap.sh](#bootstrapsh)
    + [Find\<Package\>.cmake](#find--package--cmake)
    + [find_package](#find-package)
  * [Built-in dependency](#built-in-dependency)
    + [ExternalProject_Add](#externalproject-add)
      - [`URL` and `GIT`](#-url--and--git-)
      - [`SOURCE_DIR`](#-source-dir-)
      - [`PATCH_COMMAND`](#-patch-command-)
      - [`CMAKE_ARGS`](#-cmake-args-)
      - [`BUILD_BYPRODUCTS`](#-build-byproducts-)
      - [`EXCLUDE_FROM_ALL`](#-exclude-from-all-)
      - [`LIST_SEPARATOR`](#-list-separator-)
    + [Choosing a source](#choosing-a-source)
    + [Patching](#patching)
    + [Build options](#build-options)
    + [find_package-like variables](#find-package-like-variables)
    + [Imported library targets](#imported-library-targets)
    + [Using third parties in other third parties](#using-third-parties-in-other-third-parties)
      - [Making a third party available to other third parties](#making-a-third-party-available-to-other-third-parties)
        * [Find\<Package\>.cmake](#find--package--cmake-1)
        * [Passthrough variables](#passthrough-variables)
      - [Using a third party from another third party](#using-a-third-party-from-another-third-party)
        * [Dependencies](#dependencies)
        * [CMake module path and passthrough args](#cmake-module-path-and-passthrough-args)
    + [Interface libraries](#interface-libraries)


## Choosing a third party

Deciding if a third party is needed for a particular task and if so, choosing between the different implementations is difficult. A few points that have to considered are:
 - every third party introduces risk, both operational and security
 - every third party adds a maintenance burden: it has to be tracked for issues, updated, adapted to changes in the build framework
 - not using a third party and relying on less tested homegrown solutions however usually carry a greater risk than using one
 - introducing a new third party dependency to the core should be done with the utmost care. If we make a third party a core dependency, it will increase build time, executable size and the burden to maintain API compatibility.

A few tips to choose a third party:
 - you have to choose a third party with a [proper license](#license)
 - prefer well-maintained third parties. Abandoned projects will have a huge maintenance burden.
 - prefer third parties with frequent/regular releases. There are some projects with a huge number of commits and a very long time since the last release, and we are at a disadvantage in determining whether the actual state of the master is stable: the maintainers should be the judges of that.
 - prefer third parties with the smaller number of transitive dependencies. If the third party itself needs other third parties, that increases the work greatly to get it done properly at the first time and then maintain it afterwards.

### License
Only third parties with an Apache License 2.0-compatible license may be linked with this software.

To make sure the third party's license is compatible with Apache License 2.0, refer to the [ASF 3RD PARTY LICENSE POLICY
](https://www.apache.org/legal/resolved.html). Please also note that license compatibility is a one-way street: a license may be compatible with Apache License 2.0 but not the other way round.

GPL and LGPL are generally not compatible.

## Built-in or system dependency
When deciding whether a third party dependency should be provided by the system, or compiled and shipped by us, there are many factors to consider.

|          | Advantages                                                                          | Disadvantages                                              |
|----------|-------------------------------------------------------------------------------------|------------------------------------------------------------|
| System   | Smaller executable size                                                             | Less control over third-party                              |
|          | Faster compilation                                                                  | Can't add patches                                          |
|          |                                                                                     | Has to be supported out-of-the box on all target platforms |
|          |                                                                                     | Usually not available on Windows                           |
| Built-in | High level of control over third-party (consistent version and features everywhere) | Larger executable size                                     |
|          | Can add patches                                                                     | Slower compilation                                         |
|          | Does not have to be supported by the system                                         |                                                            |
|          | Works on Windows                                                                    |                                                            |

Even if choosing a system dependency, a built-in version for Windows usually has to be made.

Both a system and a built-in version can be supported, in which case the choice should be configurable via CMake options.

**The goal is to abstract the nature of the third party from the rest of the project**, and create targets from them, that automatically take care of building or finding the third party and any dependencies, be it target, linking or include.

## System dependency

To add a new system dependency, you have to follow the following steps:

### bootstrap.sh

If you are using a system dependency, you have to ensure that the development packages are installed on the build system if the extension is selected.

To ensure this, edit `bootstrap.sh` and all the platform-specific scripts (`centos.sh`, `fedora.sh`, `debian.sh`, `suse.sh`, `rheldistro.sh`, `darwin.sh`).

### Find\<Package\>.cmake

If a `Find<Package>.cmake` is provided for your third party by not unreasonably new (not later than 3.2) CMake versions out of the box, then you have nothing further to do, unless they don't create imported library targets.

If it is not provided, you have three options
 - if a newer CMake version provides it, you can try "backporting it"
 - you can search for an already implemented one in other projects with an acceptable license
 - if everything else fails, you can write one yourself

If you don't end up writing it from scratch, make sure that you indicate the original source in the `NOTICE` file.

If you need to add a `Find<Package>.cmake` file, add it as `cmake/<package>/sys/Find<Package>.cmake`, and add it to the `CMAKE_MODULE_PATH`.

### find_package

After you have a working `Find<Package>.cmake`, you have to call `find_package` to actually find the package, most likely with the REQUIRED option to set, to make it fail if it can't find it.

Example:
```
find_package(Lib<Package> REQUIRED)
```

## Built-in dependency
We thrive to build all third party dependencies using the [External Projects](https://cmake.org/cmake/help/latest/module/ExternalProject.html) CMake feature. This has many advantages over adding the third party source to our own CMake-tree with add_subdirectory:
 - ExternalProject_Add works with non-CMake third parties
 - we have greater control over what variables are passed to the third party project
 - we don't have to patch the third parties to avoid target and variable name collisions
 - we don't have to include the third party sources in our repository

There are some exceptions to using External Projects:
 - header only libraries don't require it (you could still use ExternalProject_Add to download and unpack the sources, but it is easier to just include the source in our repository and create an INTERFACE target from them).
 - there are some libraries (notably OpenCV) which generate so many targets in so many configurations and interdependencies between the targets that it is impractical to use imported library targets with them
 - there are a few third parties that have not yet been converted to an External Project, but they will be, eventually

To add a new built-in dependency, the easiest way is to use an already existing one as a template.

You will need to do the following steps:
 - create `cmake/Bundled<Package>.cmake`
 - (optional) if you want to use this from other third parties, create `cmake/<package>/dummy/Find<Package>.cmake`
 - call the function created in `Bundled<Package>.cmake` in the main `CMakeLists.txt`:
     ```
     include(Bundled<Package>)
     use_bundled_<package>(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
     ```
     If you created `cmake/<package>/dummy/Find<Package>.cmake` you should also add that to the module path:
     ```
     list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/<package>/dummy")
     ```
     These should be in an extension's enabled conditional path, if the third party is only used by one extension, or in the section for third parties used my multiple packages, if used by more.
 - Link your extension with the imported third party targets. If everything is done right, dependencies, transitive library linkings and include paths should work automatically.

### ExternalProject_Add
`ExternalProject_Add` creates a custom target that will build the third party according to our configuration.

It has many options, some of which are described in greater detail later. Let's take a look at the most important ones:

#### `URL` and `GIT`
Used for fetching the source. In the case of `URL`, it is automatically unpacked. In the case of `GIT` the specified tag is checked out.

See [Choosing a source](#choosing-a-source) for greater detail.

Example:
```
GIT "https://github.com/<package>/<package>.git"
GIT_TAG "v1.0.0"
```
```
URL "https://github.com/<package>/<package>/archive/v1.0.0.tar.gz"
URL_HASH "SHA256=9b640b13047182761a99ce3e4f000be9687566e0828b4a72709e9e6a3ef98477"
```

#### `SOURCE_DIR`
The directory to which will be unpacked/cloned. Must be in the `BINARY_DIR`, so that we don't contaminate our source.

Example:
```
SOURCE_DIR "${BINARY_DIR}/thirdparty/package-src"
```

#### `PATCH_COMMAND`
Specifies a custom command to run after the source has been downloaded/updated. Needed for applying patches and in the case of non-CMake projects run custom scripts.

See [Patching](#patching) for greater detail.

#### `CMAKE_ARGS`
Specifies the arguments to pass to the cmake command line.

Be sure to include `${PASSTHROUGH_CMAKE_ARGS}` in this list, because it contains the basic information (compiler, build type, generator, etc.) to the third party, that must be consistent across our entire build.

See [Build options](#build-options) for greater detail.

#### `BUILD_BYPRODUCTS`
`ExternalProject_Add` needs to know the list of artifacts that are generated by the third party build (and that we care about), so that it can track their modification dates.

This can be usually set to the list of library archives generated by the third party.

Example:
```
BUILD_BYPRODUCTS "${<PACKAGE>_BYPRODUCT_DIR}/lib/lib<package>.lib"
```

#### `EXCLUDE_FROM_ALL`
This is required so that the custom target created by `ExternalProject_Add` does not get added to the default `ALL` target. This is something we generally want to avoid, as third party dependencies only make sense, if our code depends on them. We don't want them to be top-level targets and built unconditionally.

#### `LIST_SEPARATOR`
[CMake lists](https://cmake.org/cmake/help/v3.12/command/list.html#introduction) are `;` separated group of strings. When we pass `ExternalProject_Add` a list of arguments in `CMAKE_ARGS` to pass to the third party project, some of those arguments might be lists themselves (list of `CMAKE_MODULES_PATH`-s, for example), which causes issues.

To avoid this, when passing list arguments, the `;`-s should be replaced with `%`-s, and the `LIST_SEPARATOR` set to `%` (it could be an another character, but as `%` is pretty uncommon both in paths and other arguments, it is a good choice).

Even if you don't yourself use list arguments, many parts of the build infrastructure do, like exported targets, so to be safe, set this.

Example:
```
string(REPLACE ";" "%" LIST_ARGUMENT_TO_PASS "${LIST_ARGUMENT}")

[...]

LIST_SEPARATOR %
CMAKE_ARGS -DFOO=ON
           -DBAR=OFF
           "-DLIST_ARGUMENT=${LIST_ARGUMENT_TO_PASS}"
```

### Choosing a source
Prefer artifacts from the official release site or a reliable mirror. If that is not available, use the https links for releases from GitHub.

Only use a git repo in a last resort:
 - applying patches to git clones is very flaky in CMake
 - it usually takes longer to clone a git repo than to download a specific version

When using the `URL` download method, **always** use `URL_HASH` with SHA256 to verify the integrity of the downloaded artifact.

When using the `GIT` download method, prefer to use the textual tag of the release instead of the commit id as the `GIT_TAG`.

### Patching
Adding patches to a third party is sometimes necessary, but maintaining a local patch set is error-prone and takes a lot of work.

Before patching, please consider whether your goal could be achieved by other ways. Perhaps there is a CMake option that can disable the particular feature you want to comment out. If the third party is not the latest released version, there might be a fix upstream already released, and you can update the third party.

If after all you decide the best option is patching, please follow these guidelines:
 - keep the patch minimal: it is easier to maintain a smaller patch
 - separate logically different patches to separate patch files: if something is fixed upstream, it is easy to remove the specific patch file for it
 - place the patch files into the `thirdparty/<third party name>/` directory and use them from there
 - write ExternalProject_Add's patch step in a platform-independent way: the patch executable on the system is determined in the main CMakeLists.txt, you should use that. An example command looks like this:
   ```
   "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/<package>/<package>.patch"
   ```

### Build options
Both CMake and configure.sh based third parties usually come with many configuration options.
When integrating a new third party, these should be reviewed and the proper ones set.

Make sure you disable any parts that is not needed (tests, examples, unneeded features). Doing this has multiple advantages:
 - faster compilation
 - less security risk: if something is not compiled in, a vulnerability in that part can't affect us
 - greater control: e.g. we don't accidentially link with an another third party just because it was available on the system and was enabled by default

### find_package-like variables
When using imported library targets, having the variables generated by `find_package(Package)`, like `PACKAGE_FOUND`, `PACKAGE_LIBRARIES` and `PACKAGE_INCLUDE_PATHS` is not necessary, because these are already handled by the imported target's interface link and include dependencies.

However, these are usually provided by built-in packages, for multiple reasons:
 - backwards compatibility: proprietary extensions might depend on them (for already existing third parties)
 - defining these is required for importing the target, and defining its link and include interface dependencies, so we might just add them
 - if we want to export this third party to other third parties, the dummy `Find<Package>.cmake` will require these variables anyway

### Imported library targets
[Imported library targets](https://cmake.org/cmake/help/v3.2/command/add_library.html#imported-libraries) reference a library file located outside the project.

They - like other library targets - can have interface (transitive) library link, include dir and compile definitions.
These dependencies define, respectively, what other libraries should be linked with the target that links this library, what include paths should be added when compiling a target linking to this library, and what compile flags should be added when compiling a target linking to this library.

If the third party creates multiple library archives, one imported target should created for each of them, creating the proper dependencies between them, if necessary.

The imported targets should be made dependent on the target created by `ExternalProject_Add`, to make sure that we really have the proper artifacts before we want to use them.

Imported targets are customarily named like `PACKAGE::libPackage`

Unfortunately older CMake versions don't support `target_include_directories` and `target_link_libraries` for IMPORTED targets, so we have to work this around by directly interacting with the `INTERFACE_INCLUDE_DIRECTORIES` and `INTERFACE_LINK_LIBRARIES` lists of the target. Because of this, we also have to make sure that the include directory resulting from the installation of the third party is created beforehand, so that CMake won't complain about a non-existing directory.

Example:
```
file(MAKE_DIRECTORY ${PACKAGE_INCLUDE_DIRS})

add_library(PACKAGE::libHelper STATIC IMPORTED)
set_target_properties(PACKAGE::libHelper PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_INCLUDE_DIRS}")
set_target_properties(PACKAGE::libHelper PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${HELPER_LIBRARY}")
add_dependencies(PACKAGE::libHelper package-external)

add_library(PACKAGE::libPackage STATIC IMPORTED)
set_target_properties(PACKAGE::libPackage PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PACKAGE_INCLUDE_DIRS}")
set_target_properties(PACKAGE::libPackage PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${PACKAGE_LIBRARY}")
add_dependencies(PACKAGE::libPackage package-external)
set_property(TARGET PACKAGE::libPackage APPEND PROPERTY INTERFACE_LINK_LIBRARIES PACKAGE::libHelper)
```

### Using third parties in other third parties
Third party libraries can depend on other third party libraries. In this case, we obviously want all third parties to use the other third parties built by us, and not start trying to find them on the system.

To make a third party (user third party) use an another third party (provider third party), we have to
 - make sure the provider third party gets built before the user third party
 - create a `Find<Package>.cmake` file for the provider third party
 - make the user third party use this `Find<Package>.cmake` to find the provider third party
 - pass all variables used by the provider third party's `Find<Package>.cmake` to the user third party
 - if there are multiple dependencies, do this for every single one of them

This is a complex and error-prone task, so to make it easier, a helper architecture is used.

#### Making a third party available to other third parties

##### Find\<Package\>.cmake
Create `cmake/<package>/dummy/Find<Package>.cmake` like this:
```
if(NOT <PACKAGE>_FOUND)
  set(<PACKAGE>_FOUND "YES" CACHE STRING "" FORCE)
  set(<PACKAGE>_INCLUDE_DIR "${EXPORTED_<PACKAGE>_INCLUDE_DIR}" CACHE STRING "" FORCE)
  set(<PACKAGE>_LIBRARY ${EXPORTED_<PACKAGE>_LIBRARY} CACHE STRING "" FORCE)
endif()

if(NOT TARGET <PACKAGE>::lib<Package>)
  add_library(<PACKAGE>::lib<Package> STATIC IMPORTED)
  set_target_properties(<PACKAGE>::lib<Package> PROPERTIES
          INTERFACE_INCLUDE_DIRECTORIES "${<PACKAGE>_INCLUDE_DIR}")
  set_target_properties(<PACKAGE>::lib<Package> PROPERTIES
          IMPORTED_LINK_INTERFACE_LANGUAGES "C"
          IMPORTED_LOCATION "${<PACKAGE>_LIBRARY}")
endif()
```
You have to use the variables that are used by the non-dummy `Find<Package>.cmake` (and consequently used by the third party).
You only need to create imported targets here if the third party uses it instead of the variables.

Once that's done, add it to the `CMAKE_MODULE_PATH` in the main `CMakeLists.txt`:
```
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/<package>/dummy")
```

##### Passthrough variables
You will also have to supply the variables used by this `Find<Package>.cmake`.
The `PASSTHROUGH_VARIABLES` cache list is used for this: you have to append all the variables you want to pass to this list, as CMake passthrough variables.
The variables must begin with `EXPORTED_` and must be prefixed with the third party's name, to make sure there are no collisions:
```
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_<PACKAGE>_INCLUDE_DIR=${<PACKAGE>_INCLUDE_DIR}" CACHE STRING "" FORCE)
set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_<PACKAGE>_LIBRARY=${<PACKAGE>_LIBRARY}" CACHE STRING "" FORCE)
```
`PASSTHROUGH_VARIABLES` will be used by the helper function that passes all necessary variables to other third parties using this third party.

#### Using a third party from another third party

##### Dependencies
You have to make sure the third party is available before you want to use it from an another third party.
To ensure this, make the ExternalProject depend on the imported targets from the third party you want to use. This way the exact mode in which the third party will be provided is abstracted and can be adapted by the provider third party without breaking us:
```
add_dependencies(lib<package>-external FOO::libFoo BAR::libBar)
```

##### CMake module path and passthrough args
To pass our CMake module paths and the variables used by them you can use the `append_third_party_passthrough_args` helper function that will append everyhting needed to your `CMAKE_ARGS`:
```
append_third_party_passthrough_args(<PACKAGE>_CMAKE_ARGS "${<PACKAGE>_CMAKE_ARGS}")
```
Make sure you also have [`LIST_SEPARATOR`](#list_separator) set to `%`, as we pass lists here.

Unfortunately some third parties are written in a way that they override the `CMAKE_MODULE_PATH` passed to them via CMake args.
If this is the case, you will have to patch the third party and change something like this
```
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
```
to this
```
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
```

### Interface libraries
[Interface libraries](https://cmake.org/cmake/help/v3.2/manual/cmake-buildsystem.7.html#interface-libraries) can be used to create targets from header-only libraries and use them the same way as any other library target.

Header-only third party libraries are placed in the the `thirdparty` directory and an interface library target is created from them.

Example:
```
add_library(foo INTERFACE)
target_include_directories(foo INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libfoo-1.0.0")
```

# Build MiNiFi C++ with Conan & CMake

## Install MiNiFi C++ External Lib Deps with Conan

~~~bash
# conanfile.py is in root dir of MiNiFi C++ project
cd nifi-minifi-cpp

# install conan packages for MiNiFi C++ from dir conanfile.py is located
conan install . --build=missing --output-folder=build -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux

conan install . --output-folder=build -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

## Build MiNiFi C++ Project with Conan CMake

~~~bash
# install conan packages and build C++ in jam repo using Conan & CMake
conan build . --output-folder=build -pr=/home/bizon/src/jam-repo/main/etc/build/conan/profiles/release-linux
~~~

## Build MiNiFi C++ Project with CMake & Make (Caution)

I do want to mention if we build MiNiFi C++ this way using cmake generate and make, the conan cached variables we configured for the MiNiFi C++ options
will not go through when MiNiFi C++ generates the build OS files and then compiles the code. Thus, I recommend we use the conan build approach.

~~~bash
cd build
cmake ..
make
~~~
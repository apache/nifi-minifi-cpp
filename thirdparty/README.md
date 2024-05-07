# Managing 3rd Party Deps for MiNiFi C++

For MiNiFi C++, we've built all the external library dependencies using standalone CMake with FetchContent(...) and ExternalProject_Add(...) approach in the .cmake files. With some of those .cmake files, we have some of them pulling in the header files, source code, patch files, etc from the thirdparty folder. 

Now that we are also adding support for building MiNiFi C++ with conan, we're adding conan recipes to the thirdparty folder, so in cases where we need to build our own third party libraries for MiNiFi C++, we can do it using **conan create**.

We have included an example for creating the **rocksdb** conan package using **conan create** while accounting for how we added and applied the **arm7.patch** and **dboptions_equality_operator.patch** updates to that conan package. Once you try out creating this **rocksdb** conan package, you'll be familiar with how to create other conan packages for the MiNiFi C++ dependencies. 

We'll also integrate conan into the MiNiFi C++ **bootstrap.py** file to install MiNiFi C++ external library dependencies and/or build them if needed as well as then build MiNiFi C++.

## Create Conan Packages for MiNiFi C++ Dependencies

### Create BZip2 Conan Package

~~~bash
pushd nifi-minifi-cpp/thirdparty/bzip2/all
conan create . --version=1.0.8 --user=minifi --channel=dev --build=missing -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

### Create Rocksdb Conan Package

~~~bash
pushd nifi-minifi-cpp/thirdparty/rocksdb/all
conan create . --version=8.10.2 --user=minifi --channel=dev --build=missing -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

Heres an example of the **rocksdb** conan package successfully being created with the **arm7.patch** and **dboptions_equality_operator.patch** files being applied like we did in the previous standalone CMake approach for building the rocksdb library.

~~~bash
-- Using Conan toolchain: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/thirdparty/rocksdb/all/test_package/build/gcc-11-x86_64-gnu20-release/generators/conan_toolchain.cmake
-- Conan toolchain: C++ Standard 20 with extensions ON
-- The CXX compiler identification is GNU 11.4.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/lib/ccache/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Conan: Component target declared 'RocksDB::rocksdb'
-- Configuring done
-- Generating done
-- Build files have been written to: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/thirdparty/rocksdb/all/test_package/build/gcc-11-x86_64-gnu20-release

rocksdb/8.10.2@minifi/dev (test package): Running CMake.build()
rocksdb/8.10.2@minifi/dev (test package): RUN: cmake --build "/home/ubuntu/src/james/pipeline/nifi-minifi-cpp/thirdparty/rocksdb/all/test_package/build/gcc-11-x86_64-gnu20-release" -- -j20
[ 25%] Building CXX object CMakeFiles/test_package_stable_abi.dir/test_package_stable_abi.cpp.o
[ 50%] Building CXX object CMakeFiles/test_package_cpp.dir/test_package.cpp.o
[ 75%] Linking CXX executable test_package_stable_abi
[ 75%] Built target test_package_stable_abi
[100%] Linking CXX executable test_package_cpp
[100%] Built target test_package_cpp


======== Testing the package: Executing test ========
rocksdb/8.10.2@minifi/dev (test package): Running test()
rocksdb/8.10.2@minifi/dev (test package): RUN: ctest --output-on-failure -C Release
Test project /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/thirdparty/rocksdb/all/test_package/build/gcc-11-x86_64-gnu20-release
    Start 1: test_package_stable_abi
1/2 Test #1: test_package_stable_abi ..........   Passed    0.08 sec
    Start 2: test_package_cpp
2/2 Test #2: test_package_cpp .................   Passed    0.08 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   0.16 sec
~~~

### Create MBedTLS Conan Package

~~~bash
pushd nifi-minifi-cpp/thirdparty/mbedtls/all
conan create . --version=2.16.3 --user=minifi --channel=dev --build=missing -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~


### Create Open62541 Conan Package

~~~bash
pushd nifi-minifi-cpp/thirdparty/open62541/all
conan create . --version=1.3.3 --user=minifi --channel=dev --build=missing -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

## References

- [Conan 2 - Devops Guide](https://docs.conan.io/2/devops.html): Designing and implementing conan in production. Shows information on using ConanCenter packages, local recipes index repository, backing up third-party sources, managing package metadata files, versioning, saving and restoring packages from/to cache
- [Conan 2 - Commands](https://docs.conan.io/2/reference/commands.html): Shows conan commands, common commands we'll use in scripts and directly in terminal include `conan create`, `conan install`, `conan build`, `conan remove`, etc

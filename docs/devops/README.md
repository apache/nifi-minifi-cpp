# Build MiNiFi C++

- Build MiNiFi C++ with Conan & CMake
- Build MiNiFi C++ with CMake
- Appendix: Create Conan Packages for MiNiFi C++ Dependencies

We show 2 examples of building MiNiFi C++ with the first one being the quick way with conan install and conan build while the second one being the previous way with standalone cmake. The first approach is faster because we use conan 2.2.3 to manage installing the external library dependencies for MiNiFi C++ and these external libraries are represented as prebuilt C++ binary conan packages and thus no need to download source and build source external library dependencies prior to actually building the MiNiFi C++ code. The prevoius approach was building MiNiFi C++ using CMake to first download the external source libraries and build those libraries and then build MiNiFi C++.

For this particular case, we will build MiNiFi C++'s **core-minifi, minifiexe and minifi-standard-processors** and the external library dependencies and extensions these 3 core assets depend on using conan and then do it using standalone CMake. For conan, we
already configured with tc.cache_variables["{MINIFI_BUILD_ARG0}"] in MiNiFi's conanfile.py to have just these core libraries and binary executables to be built. Similarly, in the case of CMake, we pass definitions to CMake generate, so the similar effect happens. You'll see the MiNiFi C++ conan build approach is faster than the MiNiFi C++ standalone CMake build approach.



## Build MiNiFi C++ with Conan & CMake

### Install MiNiFi C++ External Lib Deps with Conan

~~~bash
# conanfile.py is in root dir of MiNiFi C++ project
cd nifi-minifi-cpp

# In case need to delete all conan packages
conan remove "*" -c

# 1st: 10:52PM - 10:52PM:

# install conan packages for MiNiFi C++ from dir conanfile.py is located and any missing prebuilt binary conan packages, build them
# only catch2's prebuilt binary conan package is missing, so it'll be built during install; we'll later upload it to our conan repo
conan install . --build=missing --output-folder=build_conan -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

### Build MiNiFi C++ Project with Conan CMake

~~~bash
# 1st SUCCESS: 10:52PM - 10:53PM:

# install conan packages and build C++ in jam repo using Conan & CMake
conan build . --output-folder=build_conan -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

## Build MiNiFi C++ with CMake

### Build MiNiFi C++ Project with CMake & Make

I do want to mention if we build MiNiFi C++ this way using cmake generate and make, the conan cached variables we configured for the MiNiFi C++ options
will not go through when MiNiFi C++ generates the build OS files and then compiles the code. Thus, I recommend we use the conan build approach.

~~~bash
cd nifi-minifi-cpp

mkdir build_cmake2
cd build_cmake2

# NOTE: libminifi-http-curl fails without CMAKE_BUILD_TYPE=Debug else cant find libcurl-d.a

#1st: 9:29PM - 9:29PM; 
#2nd: 9:33PM - 9:33PM; 
#3rd: 9:35PM - 9:35PM; 
#4th: 9:40PM - 9:40PM:
#5th: 9:54PM - 9:55PM:
#6th: 10:01PM - 10:02PM:
# 7th: 10:59PM - 10:59PM:

# building with gtests too
# 1st: 8:34PM - 8:35PM; 9:10PM - 9:10PM
# The definition args that are ON are for following:
  # NEEDED for MiNiFi C++ Core, MainExe, Standard Processors
  # NEEDED for MiNiFi C++ GTESTS (SKIP_TESTS=False, ENABLE_EXPRESSION_LANGUAGE, ENABLE_ROCKSDB, BUILD_ROCKSDB)

cmake .. -DUSE_CONAN_PACKAGER=OFF \
         -DUSE_CMAKE_FETCH_CONTENT=ON \
         -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_OPENWSMAN=ON \
         -DMINIFI_OPENSSL=ON \
         -DENABLE_CIVET=ON \
         -DENABLE_CURL=ON \
         -DSKIP_TESTS=OFF \
         -DENABLE_EXPRESSION_LANGUAGE=ON \
         -DENABLE_BZIP2=ON \
         -DENABLE_ROCKSDB=ON \
         -DBUILD_ROCKSDB=ON \
         -DENABLE_OPS=ON \
         -DENABLE_JNI=OFF \
         -DENABLE_OPC=ON \
         -DENABLE_NANOFI=ON \
         -DENABLE_SYSTEMD=ON \
         -DENABLE_PROCFS=ON \
         -DENABLE_ALL=OFF \
         -DENABLE_LIBARCHIVE=ON \
         -DENABLE_LZMA=ON \
         -DENABLE_GPS=ON \
         -DENABLE_COAP=ON \
         -DENABLE_SQL=ON \
         -DENABLE_MQTT=OFF \
         -DENABLE_PCAP=OFF \
         -DENABLE_LIBRDKAFKA=OFF \
         -DENABLE_LUA_SCRIPTING=OFF \
         -DENABLE_PYTHON_SCRIPTING=OFF \
         -DENABLE_SENSORS=OFF \
         -DENABLE_USB_CAMERA=OFF \
         -DENABLE_AWS=OFF \
         -DENABLE_OPENCV=OFF \
         -DENABLE_BUSTACHE=OFF \
         -DENABLE_SFTP=OFF \
         -DENABLE_AZURE=OFF \
         -DENABLE_ENCRYPT_CONFIG=OFF \
         -DENABLE_SPLUNK=OFF \
         -DENABLE_ELASTICSEARCH=OFF \
         -DENABLE_GCP=OFF \
         -DENABLE_KUBERNETES=OFF \
         -DENABLE_TEST_PROCESSORS=OFF \
         -DENABLE_PROMETHEUS=OFF \
         -DENABLE_GRAFANA_LOKI=OFF \
         -DENABLE_GRPC_FOR_LOKI=OFF \
         -DENABLE_CONTROLLER=OFF

#Built MiNiFi C++ Core, MainExe, Standard-Processors & Needed Deps wout GTESTs: 
# 1st FAILED: 9:29PM -9:32PM; 
# 2nd FAILED: 9:33PM - 9:35PM; 
# 3rd FAILED: 9:35PM - 9:37PM:

make -j $(nproc)

# 4th FAILED: 9:42PM -9:46PM

make

# 5th FAILED: 9:55PM - 10PM:
# 6th SUCCESS: 10:02PM - 10:04PM:
# 7th SUCCESS: 11PM - 11PM:

make -j $(nproc)

# Built with GTESTs too
# 1st SUCCESS
make -j $(nproc)
~~~

### Tracking MiNiFi C++ Standalone CMake Build Fails

For reference, while building MiNiFi C++ because we also build the external lib dependencies for building the core-minifi, minifiexe and minifi-standard-processors, we ran into these issues from the extensios on waiting for unfinished jobs and the build failed (we'll rerun MiNiFi C++ to resolve this issue):

~~~bash
[ 95%] Linking CXX shared library ../../bin/libminifi-civet-extensions.so
[ 95%] Built target minifi-civet-extensions
[ 95%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/PutFile.cpp.o
[ 95%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/PutTCP.cpp.o
[ 96%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/PutUDP.cpp.o
make[1]: *** [CMakeFiles/Makefile2:2878: extensions/openwsman/CMakeFiles/minifi-openwsman.dir/all] Error 2
make[1]: *** Waiting for unfinished jobs....
[ 96%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/ReplaceText.cpp.o
[ 96%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/RetryFlowFile.cpp.o
[ 96%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/RouteOnAttribute.cpp.o
[ 97%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/RouteText.cpp.o
make[1]: *** [CMakeFiles/Makefile2:2785: extensions/http-curl/CMakeFiles/minifi-http-curl.dir/all] Error 2
[ 97%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/SplitText.cpp.o
[ 97%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/TailFile.cpp.o
[ 98%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/processors/UpdateAttribute.cpp.o
[ 98%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/utils/JoltUtils.cpp.o
[ 98%] Building CXX object extensions/standard-processors/CMakeFiles/minifi-standard-processors.dir/__/__/ExtensionBuildInfo.cpp.o
[ 99%] Linking CXX executable ../bin/minifi
[ 99%] Built target minifiexe
[100%] Linking CXX shared library ../../bin/libminifi-standard-processors.so
[100%] Built target minifi-standard-processors
make: *** [Makefile:156: all] Error 2
~~~

We look further into this failure, we see the problem comes from libcurl:

~~~bash
ubuntu@ezedgepipeline ~/src/james/pipeline/nifi-minifi-cpp/build_cmake (MINIFICPP-2346)$ make -j $(nproc)
[  4%] Built target ossp-uuid-external-build
[  4%] Built target openssl-external
[  6%] Built target libsodium-external-build
[  9%] Built target zlib-external
[ 10%] Built target date-tz
[ 11%] Built target fmt
[ 14%] Built target yaml-cpp-external
[ 16%] Built target libxml2-external-build
[ 16%] Built target ossp-uuid-external
[ 17%] Built target libsodium-external
[ 19%] Built target curl-external
[ 20%] Built target civetweb-c-library
[ 20%] Built target libxml2-external
[ 22%] Built target spdlog
[ 25%] Built target openwsman-external
[ 25%] Built target civetweb-c-executable
[ 25%] Built target civetweb-cpp
[ 82%] Built target core-minifi
Consolidate compiler generated dependencies of target minifi-openwsman
[ 83%] Built target minifi-civet-extensions
make[2]: *** No rule to make target 'thirdparty/curl-install/lib/libcurl-d.a', needed by 'bin/libminifi-openwsman.so'.  Stop.
make[1]: *** [CMakeFiles/Makefile2:2878: extensions/openwsman/CMakeFiles/minifi-openwsman.dir/all] Error 2
make[1]: *** Waiting for unfinished jobs....
make[2]: *** No rule to make target 'thirdparty/curl-install/lib/libcurl-d.a', needed by 'bin/libminifi-http-curl.so'.  Stop.
make[1]: *** [CMakeFiles/Makefile2:2785: extensions/http-curl/CMakeFiles/minifi-http-curl.dir/all] Error 2
[ 85%] Built target minifiexe
[ 96%] Built target minifi-standard-processors
make: *** [Makefile:156: all] Error 2
~~~

My guess is libcurl failed to download using ExternalProject_Add(curl-external ...). Thus, openwsman complains about not having that dependency.

Now after building on single core, it shows the issue was coming from libminifi-http-curl.so depending on libcurl-d.a:

~~~bash
[100%] Built target libcurl_static
[ 78%] Performing install step for 'curl-external'
Consolidate compiler generated dependencies of target libcurl_static
[100%] Built target libcurl_static
Install the project...
-- Install configuration: ""
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/libcurl.a
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/bin/curl-config
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/pkgconfig/libcurl.pc
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/options.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/stdcheaders.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/curlver.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/easy.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/urlapi.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/multi.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/websockets.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/mprintf.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/header.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/system.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/curl.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/include/curl/typecheck-gcc.h
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/cmake/CURL/CURLTargets.cmake
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/cmake/CURL/CURLTargets-noconfig.cmake
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/cmake/CURL/CURLConfigVersion.cmake
-- Installing: /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/thirdparty/curl-install/lib/cmake/CURL/CURLConfig.cmake
[ 78%] Completed 'curl-external'
[ 78%] Built target curl-external
[ 79%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/HTTPCurlLoader.cpp.o
[ 79%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/client/HTTPClient.cpp.o
[ 79%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/client/HTTPStream.cpp.o
[ 80%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/processors/InvokeHTTP.cpp.o
[ 80%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/protocols/RESTSender.cpp.o
[ 80%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/sitetosite/HTTPProtocol.cpp.o
[ 81%] Building CXX object extensions/http-curl/CMakeFiles/minifi-http-curl.dir/__/__/ExtensionBuildInfo.cpp.o
make[2]: *** No rule to make target 'thirdparty/curl-install/lib/libcurl-d.a', needed by 'bin/libminifi-http-curl.so'.  Stop.
make[1]: *** [CMakeFiles/Makefile2:2785: extensions/http-curl/CMakeFiles/minifi-http-curl.dir/all] Error 2
make: *** [Makefile:156: all] Error 2
~~~

NOTE: I found the issue about why we get libminifi-http-curl.so failing and expecting libcurl-d.a is because we dont build minifi cpp in debug mode. However, if we want to build minifi cpp in release mode, libminifi-http-curl should be able to take either build type and succeed. For now, I will build minifi cpp in debug mode


Heres another problem that comes up from downloading the zlib external lib:

~~~bash
[ 17%] Completed 'libsodium-external'
[ 17%] Built target libsodium-external

CMake Error at /home/ubuntu/src/james/pipeline/nifi-minifi-cpp/build_cmake/zlib-external-prefix/src/zlib-external-stamp/download-zlib-external.cmake:170 (message):
  Each download failed!

    error: downloading 'https://github.com/madler/zlib/archive/v1.2.11.tar.gz' failed
          status_code: 28
          status_string: "Timeout was reached"
          log:
          --- LOG BEGIN ---
            Trying 140.82.116.3:443...

  Connected to github.com (140.82.116.3) port 443 (#0)

  ALPN: offers h2

  ALPN: offers http/1.1

  TLSv1.0 (OUT), TLS header, Certificate Status (22):

  [5 bytes data]

  TLSv1.3 (OUT), TLS handshake, Client hello (1):

  [512 bytes data]

  SSL connection timeout

  Closing connection 0

  

          --- LOG END ---
~~~

## Appendix: Create Conan Packages for MiNiFi C++ Dependencies

~~~bash
pushd nifi-minifi-cpp/thirdparty/rocksdb/all
conan create . --version=8.10.2 --build=missing -pr=$HOME/src/james/pipeline/nifi-minifi-cpp/etc/build/conan/profiles/release-linux
~~~

More information on our conan packages: **`nifi-minifi-cpp/thirdparty/README.md`**

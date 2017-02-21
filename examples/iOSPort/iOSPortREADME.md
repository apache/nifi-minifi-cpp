1) Install XCode
2) Build libminfi.a
cd libminifi, mkdir lib, cd lib
cmake -DCMAKE_TOOLCHAIN_FILE=../../cmake/iOS.cmake -DIOS_PLATFORM=SIMULATOR64 ..
make
after that it will create libminifi.a for Xcode Simulator
3) Create a XCode iphone simulator project, in build stage add link library for libminfi.a
4) In IOS project file, you can call any MiNiFi APIs listed in libminifi/include  

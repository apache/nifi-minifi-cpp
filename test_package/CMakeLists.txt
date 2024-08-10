cmake_minimum_required(VERSION 3.15)
project(test-minifi-cpp CXX)

find_package(minifi-cpp CONFIG REQUIRED)
find_package(gsl-lite CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_executable(abstract_processor_test src/AbstractProcessorTest.cpp)

target_link_libraries(abstract_processor_test 
minifi-cpp::minifi-cpp
gsl::gsl-lite
fmt::fmt
spdlog::spdlog
Catch2::Catch2WithMain
)

list(APPEND gsl-lite_DEFINITIONS_RELEASE gsl_CONFIG_DEFAULTS_VERSION=1)

target_compile_definitions(gsl::gsl-lite INTERFACE ${gsl-lite_DEFINITIONS_RELEASE})

target_include_directories(abstract_processor_test PUBLIC
    ${fmt_INCLUDE_DIRS}
    ${minifi-cpp_INCLUDE_DIRS}
    ${minifi-cpp_INCLUDE_DIRS}/libminifi/include
    ${minifi-cpp_INCLUDE_DIRS}/libminifi/test/libtest
    ${minifi-cpp_INCLUDE_DIRS}/_deps/magic_enum-src/include
    ${minifi-cpp_INCLUDE_DIRS}/_deps/date_src-src/include
    ${minifi-cpp_INCLUDE_DIRS}/_deps/catch2-src/src
    ${minifi-cpp_INCLUDE_DIRS}/_deps/catch2-build/generated-includes
    ${minifi-cpp_INCLUDE_DIRS}/_deps/spdlog-src/include
    ${minifi-cpp_INCLUDE_DIRS}/thirdparty/rapidjson-48fbd8cd202ca54031fe799db2ad44ffa8e77c13/include
    ${minifi-cpp_INCLUDE_DIRS}/extensions/standard-processors/processors
)

add_test(NAME abstract_processor_test COMMAND abstract_processor_test)

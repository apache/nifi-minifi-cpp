# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

function(use_bundled_opencv SOURCE_DIR BINARY_DIR)
    if (WIN32)
        set(LIBDIR "lib")
    else()
        include(GNUInstallDirs)
        string(REPLACE "/" ";" LIBDIR_LIST ${CMAKE_INSTALL_LIBDIR})
        list(GET LIBDIR_LIST 0 LIBDIR)
    endif()

    # Define byproducts
    if (WIN32)
        set(SUFFIX "410.lib")
        set(THIRDPARTY_SUFFIX ".lib")
        set(PREFIX "")
        set(THIRDPARTY_DIR "")
    else()
        set(SUFFIX ".a")
        set(THIRDPARTY_SUFFIX ".a")
        set(PREFIX "lib")
        set(THIRDPARTY_DIR "opencv4/3rdparty/")
    endif()

    set(OPENCV_BYPRODUCT_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/opencv-install")
    set(BYPRODUCTS
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_flann${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_dnn${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_objdetect${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_core${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_gapi${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_imgcodecs${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_calib3d${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_imgproc${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_photo${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_videoio${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_video${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_stitching${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_features2d${SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${THIRDPARTY_DIR}${PREFIX}libjpeg-turbo${THIRDPARTY_SUFFIX}"
        "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${THIRDPARTY_DIR}${PREFIX}libpng${THIRDPARTY_SUFFIX}")

    # Set build options
    set(OPENCV_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${OPENCV_BYPRODUCT_DIR}"
            "-DBUILD_SHARED_LIBS=OFF"
            "-DBUILD_WITH_STATIC_CRT=OFF"
            "-DBUILD_EXAMPLES=OFF"
            "-DBUILD_DOCS=OFF"
            "-DBUILD_PACKAGE=OFF"
            "-DBUILD_opencv_apps=OFF"
            "-DBUILD_PERF_TESTS=OFF"
            "-DBUILD_TESTS=OFF"
            "-DBUILD_opencv_calib3d=ON"
            "-DBUILD_opencv_core=ON"
            "-DBUILD_opencv_dnn=ON"
            "-DBUILD_opencv_features2d=ON"
            "-DBUILD_opencv_flann=ON"
            "-DBUILD_opencv_gapi=ON"
            "-DBUILD_opencv_highgui=OFF"
            "-DBUILD_opencv_imgcodecs=ON"
            "-DBUILD_opencv_imgproc=ON"
            "-DBUILD_opencv_ml=OFF"
            "-DBUILD_opencv_objdetect=ON"
            "-DBUILD_opencv_photo=ON"
            "-DBUILD_opencv_stitching=ON"
            "-DBUILD_opencv_video=ON"
            "-DBUILD_opencv_videoio=ON"
            "-DBUILD_JAVA=OFF"
            "-DBUILD_FAT_JAVA_LIB=OFF"
            "-DBUILD_PNG=ON"
            "-DBUILD_JPEG=ON"
            "-DBUILD_OPENJPEG=OFF"
            "-DWITH_1394=OFF"
            "-DWITH_FFMPEG=OFF"
            "-DWITH_GSTREAMER=OFF"
            "-DWITH_GTK=OFF"
            "-DWITH_IPP=OFF"
            "-DWITH_JASPER=OFF"
            "-DWITH_OPENEXR=OFF"
            "-DWITH_ITT=OFF"
            "-DWITH_OPENEXR=OFF"
            "-DWITH_WEBP=OFF"
            "-DWITH_OPENJPEG=OFF"
            "-DWITH_TIFF=OFF"
            "-DCMAKE_CXX_STANDARD=17"  # OpenCV fails to build in C++20 mode on Clang-14
    )

    append_third_party_passthrough_args(OPENCV_CMAKE_ARGS "${OPENCV_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            opencv-external
            GIT_REPOSITORY "https://github.com/opencv/opencv.git"
            GIT_TAG "4.5.5"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/opencv-src"
            CMAKE_ARGS ${OPENCV_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BYPRODUCTS}"
            EXCLUDE_FROM_ALL TRUE
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
    )

    # Set variables
    set(OPENCV_FOUND "YES" CACHE STRING "" FORCE)
    if (WIN32)
        set(OPENCV_INCLUDE_DIR "${OPENCV_BYPRODUCT_DIR}/include" CACHE STRING "" FORCE)
    else()
        set(OPENCV_INCLUDE_DIR "${OPENCV_BYPRODUCT_DIR}/include/opencv4" CACHE STRING "" FORCE)
    endif()
    set(OPENCV_LIBRARIES "${BYPRODUCTS}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENCV_INCLUDE_DIR=${OPENCV_INCLUDE_DIR}" CACHE STRING "" FORCE)
    string(REPLACE ";" "%" OPENCV_LIBRARIES_EXPORT "${OPENCV_LIBRARIES}")
    set(PASSTHROUGH_VARIABLES ${PASSTHROUGH_VARIABLES} "-DEXPORTED_OPENCV_LIBRARIES=${OPENCV_LIBRARIES_EXPORT}" CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libjpeg-turbo STATIC IMPORTED)
    set_target_properties(OPENCV::libjpeg-turbo PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${THIRDPARTY_DIR}${PREFIX}libjpeg-turbo${THIRDPARTY_SUFFIX}")
    add_dependencies(OPENCV::libjpeg-turbo opencv-external)

    add_library(OPENCV::libpng STATIC IMPORTED)
    set_target_properties(OPENCV::libpng PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${THIRDPARTY_DIR}${PREFIX}libpng${THIRDPARTY_SUFFIX}")
    add_dependencies(OPENCV::libpng opencv-external)

    add_library(OPENCV::libopencv-core STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-core PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_core${SUFFIX}")
    add_dependencies(OPENCV::libopencv-core opencv-external ZLIB::ZLIB)
    target_include_directories(OPENCV::libopencv-core INTERFACE ${OPENCV_INCLUDE_DIR})
    target_link_libraries(OPENCV::libopencv-core INTERFACE ZLIB::ZLIB)

    add_library(OPENCV::libopencv-flann STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-flann PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_flann${SUFFIX}")
    add_dependencies(OPENCV::libopencv-flann opencv-external)
    target_include_directories(OPENCV::libopencv-flann INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-dnn STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-dnn PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_dnn${SUFFIX}")
    add_dependencies(OPENCV::libopencv-dnn opencv-external)
    target_include_directories(OPENCV::libopencv-dnn INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-objdetect STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-objdetect PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_objdetect${SUFFIX}")
    add_dependencies(OPENCV::libopencv-objdetect opencv-external)
    target_include_directories(OPENCV::libopencv-objdetect INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-gapi STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-gapi PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_gapi${SUFFIX}")
    add_dependencies(OPENCV::libopencv-gapi opencv-external)
    target_include_directories(OPENCV::libopencv-gapi INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-imgcodecs STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-imgcodecs PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_imgcodecs${SUFFIX}")
    add_dependencies(OPENCV::libopencv-imgcodecs opencv-external)
    target_include_directories(OPENCV::libopencv-imgcodecs INTERFACE ${OPENCV_INCLUDE_DIR})
    target_link_libraries(OPENCV::libopencv-imgcodecs INTERFACE OPENCV::libopencv-core OPENCV::libjpeg-turbo OPENCV::libpng)

    add_library(OPENCV::libopencv-calib3d STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-calib3d PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_calib3d${SUFFIX}")
    add_dependencies(OPENCV::libopencv-calib3d opencv-external)
    target_include_directories(OPENCV::libopencv-calib3d INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-imgproc STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-imgproc PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_imgproc${SUFFIX}")
    add_dependencies(OPENCV::libopencv-imgproc opencv-external)
    target_include_directories(OPENCV::libopencv-imgproc INTERFACE ${OPENCV_INCLUDE_DIR})
    target_link_libraries(OPENCV::libopencv-imgproc INTERFACE OPENCV::libopencv-core)

    add_library(OPENCV::libopencv-photo STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-photo PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_photo${SUFFIX}")
    add_dependencies(OPENCV::libopencv-photo opencv-external)
    target_include_directories(OPENCV::libopencv-photo INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-videoio STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-videoio PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_videoio${SUFFIX}")
    add_dependencies(OPENCV::libopencv-videoio opencv-external)
    target_include_directories(OPENCV::libopencv-videoio INTERFACE ${OPENCV_INCLUDE_DIR})
    target_link_libraries(OPENCV::libopencv-videoio INTERFACE OPENCV::libopencv-core)

    add_library(OPENCV::libopencv-video STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-video PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_video${SUFFIX}")
    add_dependencies(OPENCV::libopencv-video opencv-external)
    target_include_directories(OPENCV::libopencv-video INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-stitching STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-stitching PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_stitching${SUFFIX}")
    add_dependencies(OPENCV::libopencv-stitching opencv-external)
    target_include_directories(OPENCV::libopencv-stitching INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv-features2d STATIC IMPORTED)
    set_target_properties(OPENCV::libopencv-features2d PROPERTIES IMPORTED_LOCATION "${OPENCV_BYPRODUCT_DIR}/${LIBDIR}/${PREFIX}opencv_features2d${SUFFIX}")
    add_dependencies(OPENCV::libopencv-features2d opencv-external)
    target_include_directories(OPENCV::libopencv-features2d INTERFACE ${OPENCV_INCLUDE_DIR})

    add_library(OPENCV::libopencv INTERFACE IMPORTED)
    target_link_libraries(OPENCV::libopencv INTERFACE OPENCV::libopencv-flann OPENCV::libopencv-dnn OPENCV::libopencv-objdetect OPENCV::libopencv-core OPENCV::libopencv-gapi OPENCV::libopencv-imgcodecs OPENCV::libopencv-calib3d OPENCV::libopencv-imgproc OPENCV::libopencv-photo OPENCV::libopencv-videoio OPENCV::libopencv-video OPENCV::libopencv-stitching OPENCV::libopencv-features2d)

endfunction(use_bundled_opencv)

#
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
#

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES  "*.cpp")

set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/opencv")
set(BYPRODUCT "${CMAKE_CURRENT_BINARY_DIR}/opencv-install/")

set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/opencv")
set(BYPRODUCT "${BASE_DIR}/install")

# OpenCV 4.1.0
build_git_project(opencv-external "${BASE_DIR}" "https://github.com/opencv/opencv.git" "371bba8f54560b374fbcd47e7e02f015ac4969ad"
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_JAVA=OFF
        -DBUILD_FAT_JAVA_LIB=OFF
        -DCMAKE_INSTALL_PREFIX=${BYPRODUCT}
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_DOCS=OFF
        -DBUILD_PACKAGE=OFF
        -DBUILD_opencv_apps=OFF
        -DBUILD_PERF_TESTS=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_opencv_calib3d=ON
        -DBUILD_opencv_core=ON
        -DBUILD_opencv_dnn=ON
        -DBUILD_opencv_features2d=ON
        -DBUILD_opencv_flann=ON
        -DBUILD_opencv_gapi=ON
        -DBUILD_opencv_highgui=OFF
        -DBUILD_opencv_imgcodecs=ON
        -DBUILD_opencv_imgproc=ON
        -DBUILD_opencv_ml=OFF
        -DBUILD_opencv_objdetect=ON
        -DBUILD_opencv_photo=ON
        -DBUILD_opencv_stitching=ON
        -DBUILD_opencv_video=ON
        -DBUILD_opencv_videoio=ON)

add_library(minifi-opencv STATIC ${SOURCES})

set(OpenCV_DIR ${BASE_DIR})
find_package( OpenCV REQUIRED PATHS "${BASE_DIR}/opencv-external/src/opencv-external-build/")
target_link_libraries( minifi-opencv ${OpenCV_LIBS} )
include_directories(${OpenCV_INCLUDE_DIRS})

SET (OPENCV-EXTENSION minifi-opencv PARENT_SCOPE)
register_extension(minifi-opencv)
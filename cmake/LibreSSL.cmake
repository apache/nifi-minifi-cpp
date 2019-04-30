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

function(use_libre_ssl SOURCE_DIR BINARY_DIR)
	message("Using bundled LibreSSL from release")
	
	set(BYPRODUCT_PREFIX "lib" CACHE STRING "" FORCE)
	set(BYPRODUCT_SUFFIX ".a" CACHE STRING "" FORCE)
	
	set(BUILD_ARGS "")
	if (WIN32)
		set(BYPRODUCT_SUFFIX ".lib" CACHE STRING "" FORCE)
		set(BYPRODUCT_PREFIX "" CACHE STRING "" FORCE)
	set(BUILD_ARGS " -GVisual Studio 15 2017")
	endif(WIN32)
	ExternalProject_Add(
	libressl-portable
	URL "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.8.3.tar.gz"
	SOURCE_DIR "${BINARY_DIR}/thirdparty/libressl-src"
	CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
				"-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libressl-install"
				"-DLIBRESSL_APPS=OFF"
				"-DLIBRESSL_TESTS=OFF"
				"${BUILD_ARGS}"
	)

	add_library(crypto STATIC IMPORTED)
	set_target_properties(crypto PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}")
	add_dependencies(crypto libressl-portable)
					
	add_library(ssl STATIC IMPORTED)
	set_target_properties(ssl PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}")
	set_target_properties(ssl PROPERTIES INTERFACE_LINK_LIBRARIES crypto)
	add_dependencies(ssl libressl-portable)
	
	add_library(tls STATIC IMPORTED)
	set_target_properties(tls PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}tls${BYPRODUCT_SUFFIX}")
	set_target_properties(tls PROPERTIES INTERFACE_LINK_LIBRARIES crypto)
	add_dependencies(tls libressl-portable)
	
	set(LIBRESSL_SRC_DIR "${SOURCE_DIR}/thirdparty/libressl/" CACHE STRING "" FORCE)
	set(LIBRESSL_BIN_DIR "${BINARY_DIR}/thirdparty/libressl-install/" CACHE STRING "" FORCE)

	set(OPENSSL_FOUND "YES" CACHE STRING "" FORCE)
	set(OPENSSL_INCLUDE_DIR "${SOURCE_DIR}/thirdparty/libressl/include" CACHE STRING "" FORCE)
	set(OPENSSL_LIBRARIES "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}tls${BYPRODUCT_SUFFIX}" "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}ssl${BYPRODUCT_SUFFIX}" "${BINARY_DIR}/thirdparty/libressl-install/lib/${BYPRODUCT_PREFIX}crypto${BYPRODUCT_SUFFIX}" CACHE STRING "" FORCE)
	
endfunction(use_libre_ssl) 
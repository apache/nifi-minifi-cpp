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

# Dummy OpenSSL find for when we use bundled version

set(OPENSSL_FOUND "YES" CACHE STRING "" FORCE)
set(OPENSSL_INCLUDE_DIR "${LIBRESSL_SRC_DIR}/include" CACHE STRING "" FORCE)
set(OPENSSL_CRYPTO_LIBRARY "${LIBRESSL_BIN_DIR}/lib/libcrypto.a" CACHE STRING "" FORCE)
set(OPENSSL_SSL_LIBRARY "${LIBRESSL_BIN_DIR}/lib/libssl.a" CACHE STRING "" FORCE)
set(OPENSSL_LIBRARIES "${LIBRESSL_BIN_DIR}/lib/libtls.a" ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} CACHE STRING "" FORCE)

 if(NOT TARGET OpenSSL::Crypto )
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    
      set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}")
    
  endif()

  if(NOT TARGET OpenSSL::SSL
      )
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
          set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}")
    
  endif()
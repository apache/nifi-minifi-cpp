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

if(NOT MBEDTLS_FOUND)
    set(MBEDTLS_FOUND "YES" CACHE STRING "" FORCE)
    set(MBEDTLS_INCLUDE_DIRS "${EXPORTED_MBEDTLS_INCLUDE_DIRS}" CACHE STRING "" FORCE)
    set(MBEDTLS_LIBRARIES ${EXPORTED_MBEDTLS_LIBRARIES} CACHE STRING "" FORCE)
    set(MBEDTLS_LIBRARY "${EXPORTED_MBEDTLS_LIBRARY}" CACHE STRING "" FORCE)
    set(MBEDX509_LIBRARY "${EXPORTED_MBEDX509_LIBRARY}" CACHE STRING "" FORCE)
    set(MBEDCRYPTO_LIBRARY "${EXPORTED_MBEDCRYPTO_LIBRARY}" CACHE STRING "" FORCE)
endif()
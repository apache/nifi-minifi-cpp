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

# Resolve the concrete static-library targets to pass to $<TARGET_FILE:...> for ITEM.
# A conan package exposes each library through an INTERFACE target whose actual
# IMPORTED archive is referenced in INTERFACE_LINK_LIBRARIES as a CONAN_LIB::* target.
# Only used on Apple/Windows, where whole-archiving needs an explicit file.
function(_resolve_wholearchive_libs ITEM OUT_LIBS)
    if(TARGET ${ITEM})
        get_target_property(_item_type ${ITEM} TYPE)
        if(_item_type STREQUAL "INTERFACE_LIBRARY")
            get_target_property(_interface_libs ${ITEM} INTERFACE_LINK_LIBRARIES)
            set(_resolved "")
            if(_interface_libs)
                foreach(_entry IN LISTS _interface_libs)
                    string(REGEX MATCHALL "CONAN_LIB::[A-Za-z0-9._-]+" _matches "${_entry}")
                    list(APPEND _resolved ${_matches})
                endforeach()
            endif()
            if(NOT _resolved)
                message(FATAL_ERROR "target_wholearchive_library: could not resolve a concrete static library for interface target '${ITEM}'")
            endif()
            set(${OUT_LIBS} ${_resolved} PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${OUT_LIBS} ${ITEM} PARENT_SCOPE)
endfunction()

function(target_wholearchive_library TARGET ITEM)
    if (APPLE)
        target_link_libraries(${TARGET} ${ITEM})
        _resolve_wholearchive_libs(${ITEM} _libs)
        foreach(_lib IN LISTS _libs)
            target_link_libraries(${TARGET} -Wl,-force_load,$<TARGET_FILE:${_lib}>)
        endforeach()
    elseif(WIN32)
        target_link_libraries(${TARGET} ${ITEM})
        _resolve_wholearchive_libs(${ITEM} _libs)
        foreach(_lib IN LISTS _libs)
            target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${_lib}>")
        endforeach()
    else()
        target_link_libraries(${TARGET} -Wl,--whole-archive ${ITEM} -Wl,--no-whole-archive)
    endif()
endfunction(target_wholearchive_library)

function(target_wholearchive_library_private TARGET ITEM)
    if (APPLE)
        target_link_libraries(${TARGET} PRIVATE ${ITEM})
        _resolve_wholearchive_libs(${ITEM} _libs)
        foreach(_lib IN LISTS _libs)
            target_link_libraries(${TARGET} PRIVATE -Wl,-force_load,$<TARGET_FILE:${_lib}>)
        endforeach()
    elseif(WIN32)
        target_link_libraries(${TARGET} PRIVATE ${ITEM})
        _resolve_wholearchive_libs(${ITEM} _libs)
        foreach(_lib IN LISTS _libs)
            target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${_lib}>")
        endforeach()
    else()
        target_link_libraries(${TARGET} PRIVATE -Wl,--whole-archive ${ITEM} -Wl,--no-whole-archive)
    endif()
    if(TARGET ${ITEM})
        get_target_property(_item_type ${ITEM} TYPE)
        if(NOT _item_type STREQUAL "INTERFACE_LIBRARY")
            add_dependencies(${TARGET} ${ITEM})
        endif()
    endif()
endfunction(target_wholearchive_library_private)

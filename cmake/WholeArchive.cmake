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

function(target_wholearchive_library TARGET ITEM)
    if (APPLE)
        target_link_libraries(${TARGET} ${ITEM})
        target_link_libraries(${TARGET} -Wl,-force_load,$<TARGET_FILE:${ITEM}>)
        add_dependencies(${TARGET} ${ITEM})
    elseif(WIN32)
        target_link_libraries(${TARGET} ${ITEM})
        if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
            set_property(TARGET ${TARGET} APPEND_STRING PROPERTY LINK_FLAGS " /WHOLEARCHIVE:${ITEM}")
        else()
            target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:${ITEM}")
        endif()
        add_dependencies(${TARGET} ${ITEM})
    else()
        target_link_libraries(${TARGET} -Wl,--whole-archive ${ITEM} -Wl,--no-whole-archive)
        add_dependencies(${TARGET} ${ITEM})
    endif()
endfunction(target_wholearchive_library)

function(target_wholearchive_library_private TARGET ITEM)
    if (APPLE)
        target_link_libraries(${TARGET} PRIVATE ${ITEM})
        target_link_libraries(${TARGET} PRIVATE -Wl,-force_load,$<TARGET_FILE:${ITEM}>)
        add_dependencies(${TARGET} ${ITEM})
    elseif(WIN32)
        target_link_libraries(${TARGET} PRIVATE ${ITEM})
        if(${CMAKE_VERSION} VERSION_LESS "3.13.0")
            set_property(TARGET ${TARGET} APPEND_STRING PROPERTY LINK_FLAGS " /WHOLEARCHIVE:$<TARGET_FILE:${ITEM}>")
        else()
            target_link_options(${TARGET} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${ITEM}>")
        endif()
        add_dependencies(${TARGET} ${ITEM})
    else()
        target_link_libraries(${TARGET} PRIVATE -Wl,--whole-archive ${ITEM} -Wl,--no-whole-archive)
        add_dependencies(${TARGET} ${ITEM})
    endif()
endfunction(target_wholearchive_library_private)

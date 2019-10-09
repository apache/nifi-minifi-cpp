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


define_property(GLOBAL PROPERTY EXTENSION-OPTIONS
    BRIEF_DOCS "Global extension list"
    FULL_DOCS "Global extension list")

set_property(GLOBAL PROPERTY EXTENSION-OPTIONS "")

macro(register_extension extension-name)
  get_property(extensions GLOBAL PROPERTY EXTENSION-OPTIONS)
  set_property(GLOBAL APPEND PROPERTY EXTENSION-OPTIONS ${extension-name})
  target_compile_definitions(${extension-name} PRIVATE "MODULE_NAME=${extension-name}")
endmacro()

### TESTING MACROS

define_property(GLOBAL PROPERTY EXTENSION-TESTS
    BRIEF_DOCS "Global extension tests"
    FULL_DOCS "Global extension tests")

set_property(GLOBAL PROPERTY EXTENSION-TESTS "")

macro(register_extension_test extension-dir) 
  if (NOT SKIP_TESTS)
  	get_property(extensions GLOBAL PROPERTY EXTENSION-TESTS)
  	set_property(GLOBAL APPEND PROPERTY EXTENSION-TESTS "${extension-dir}")
  endif()
endmacro()

function(registerTest dirName)
	if (NOT SKIP_TESTS)
		add_subdirectory(${dirName})
	endif()
endfunction(registerTest)

### FUNCTION TO CREATE AN EXTENSION

function(createExtension extensionGuard extensionName description dirName)
	add_subdirectory(${dirName})
	ADD_FEATURE_INFO("${extensionName}" ${extensionGuard} "${description}")
    mark_as_advanced(${extensionGuard})
    if (ARGV4)
    	register_extension_test(${ARGV4})
    endif(ARGV4)
    if (ARGV5 AND ARGV6)
    	if (${ARGV5})
			add_subdirectory(${ARGV6})	
		endif()
    endif()
endfunction()


macro(register_extension_linter target-name)
if (NOT WIN32)
    get_property(extensions GLOBAL PROPERTY EXTENSION-LINTERS)
    set_property(GLOBAL APPEND PROPERTY EXTENSION-LINTERS "${target-name}")
    add_custom_target(${target-name}
    COMMAND ${CMAKE_SOURCE_DIR}/thirdparty/google-styleguide/run_linter.sh
            ${CMAKE_SOURCE_DIR}/libminifi/include/
            ${CMAKE_CURRENT_LIST_DIR}/ --
            ${CMAKE_CURRENT_LIST_DIR}/)
endif(NOT WIN32)
endmacro()

# ARGN WILL be the
function (build_git_project target prefix repourl repotag)

	set(exec_dir ${CMAKE_BINARY_DIR}/force_${target})

	file(MAKE_DIRECTORY ${exec_dir} ${exec_dir}/build)

	set(CMAKE_LIST_CONTENT "
        include(ExternalProject)
        ExternalProject_add(${target}
            PREFIX ${prefix}/${target}
            GIT_REPOSITORY ${repourl}
        	GIT_TAG ${repotag}
            CMAKE_ARGS \"${ARGN}\"
            INSTALL_COMMAND \"\"
            )
         add_custom_target(exec_${target})
        add_dependencies(exec_${target} ${target})
    ")

	file(WRITE ${exec_dir}/CMakeLists.txt "${CMAKE_LIST_CONTENT}")

	# Try to determine the number of CPUs and do a parallel build based on that
	include(ProcessorCount OPTIONAL RESULT_VARIABLE PROCESSCOUNT_RESULT)
	if(NOT PROCESSCOUNT_RESULT EQUAL NOTFOUND)
		ProcessorCount(NUM_CPU)
		math(EXPR PARALLELISM "${NUM_CPU} / 2")
	endif()
	if(NOT PARALLELISM OR PARALLELISM LESS 1)
		set(PARALLELISM 1)
	endif()

	message("Building ${target} with a parallelism of ${PARALLELISM}")
	execute_process(COMMAND ${CMAKE_COMMAND} ..
			WORKING_DIRECTORY ${exec_dir}/build
			)
	if(${CMAKE_VERSION} VERSION_EQUAL "3.12.0" OR ${CMAKE_VERSION} VERSION_GREATER "3.12.0")
		execute_process(COMMAND ${CMAKE_COMMAND} --build . --parallel ${PARALLELISM}
				WORKING_DIRECTORY ${exec_dir}/build
				)
	else()
		execute_process(COMMAND ${CMAKE_COMMAND} --build .
				WORKING_DIRECTORY ${exec_dir}/build
				)
	endif()
endfunction()

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License

CFLAGS = -Wall
INCLUDES = -I./include

CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(addprefix build/,$(notdir $(CPP_FILES:.cpp=.o)))

all: lib/libyaml-cpp.a

lib:
	mkdir -p ./lib

build:
	mkdir -p ./build

lib/libyaml-cpp.a: $(OBJ_FILES)
	mkdir -p ./lib
	ar crs $@ $^

build/%.o: src/%.cpp
	mkdir -p ./build
	g++ -Os $(INCLUDES) $(CC_FLAGS) -c -o $@ $<

clean:
	rm -rf ./lib ./build

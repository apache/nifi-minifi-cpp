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

CC=g++
AR=ar
BUILD_DIR= ./build
TARGET_DIR=./target
TARGET_LIB=libminifi.a
TARGET_EXE=minifi
CFLAGS=-O0 -fexceptions -fpermissive -Wno-write-strings -std=c++11 -fPIC -Wall -g -Wno-unused-private-field
INCLUDES=-I./inc -I./src -I./test -I/usr/include/libxml2 -I/usr/local/opt/leveldb/include/
LDDIRECTORY=-L/usr/local/opt/leveldb/lib/ -L./build
LDFLAGS=-lminifi -lxml2 -lleveldb -pthread -luuid

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
endif

OBJS:=$(shell /bin/ls src/*.cpp | xargs -n1 basename 2>/dev/null |  awk '/\.cpp$$/{a=$$0; gsub("\\.cpp$$",".o", a); print "$(BUILD_DIR)/" a}')
TESTS:=FlowFileRecordTest

all: directory $(BUILD_DIR)/$(TARGET_LIB) minifi tests

directory:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	$(AR) crs $@ $(OBJS)

minifi: $(BUILD_DIR)/$(TARGET_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILD_DIR)/$(TARGET_EXE) main/MiNiFiMain.cpp $(LDDIRECTORY) $(LDFLAGS)
	cp $(BUILD_DIR)/$(TARGET_EXE) $(TARGET_DIR)/$(TARGET_EXE)

tests: $(BUILD_DIR)/$(TARGET_LIB)
	$(foreach TEST_NAME, $(TESTS),\
	$(CC) $(CFLAGS) $(INCLUDES) -o $(BUILD_DIR)/$(TEST_NAME) test/$(TEST_NAME).cpp $(LDDIRECTORY) $(LDFLAGS);)

clean:
	rm -rf $(BUILD_DIR)/*
	rm -rf $(TARGET_DIR)/$(TARGET_EXE)
	cp -r $(TARGET_DIR)/conf $(BUILD_DIR)/
	 

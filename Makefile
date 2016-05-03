CC=g++
AR=ar
TARGET_DIR= ./build
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

OBJS:=$(shell /bin/ls src/*.cpp | xargs -n1 basename 2>/dev/null |  awk '/\.cpp$$/{a=$$0; gsub("\\.cpp$$",".o", a); print "$(TARGET_DIR)/" a}')
TESTS:=FlowFileRecordTest

all: directory $(TARGET_DIR)/$(TARGET_LIB) minifi tests

directory:
	mkdir -p $(TARGET_DIR)

$(TARGET_DIR)/%.o: src/%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(TARGET_DIR)/$(TARGET_LIB): $(OBJS)
	$(AR) crs $@ $(OBJS)

minifi: $(TARGET_DIR)/$(TARGET_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET_DIR)/$(TARGET_EXE) main/MiNiFiMain.cpp $(LDDIRECTORY) $(LDFLAGS)

tests: $(TARGET_DIR)/$(TARGET_LIB)
	$(foreach TEST_NAME, $(TESTS),\
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET_DIR)/$(TEST_NAME) test/$(TEST_NAME).cpp $(LDDIRECTORY) $(LDFLAGS);)

clean:
	rm -rf $(TARGET_DIR)/*

/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _WIN32
#ifndef NANOFI_TESTS_CTESTSBASE_H_
#define NANOFI_TESTS_CTESTSBASE_H_

#include <vector>
#include <string>
#include <fstream>
#include <assert.h>
#include <sys/stat.h>

#include "core/file_utils.h"
#include "api/ecu.h"
#include "api/nanofi.h"

class FileManager {
public:
    FileManager(const std::string& filePath) {
        assert(!filePath.empty() && "filePath provided cannot be empty!");
        struct stat statbuff;
        assert(!is_directory(filePath.c_str()) && "Provided file is not a filepath");
        filePath_ = filePath;
        remove(filePath_.c_str());
        outputStream_.open(filePath_, std::ios::binary);
    }

    ~FileManager() {
        std::ifstream ifs(filePath_);
        if (ifs.good()) {
            remove(filePath_.c_str());
        }
    }

    void Write(const std::string& str) {
        outputStream_ << str;
    }

    std::string WriteNChars(uint64_t n, char c) {
        std::string s(n, c);
        outputStream_ << s;
        return s;
    }

    std::string getFilePath() const {
        return filePath_;
    }

    void CloseStream() {
        outputStream_.flush();
        outputStream_.close();
    }

    uint64_t GetFileSize() {
        CloseStream();
        struct stat buff;
        if (stat(filePath_.c_str(), &buff) == 0) {
            return buff.st_size;
        }
        return 0;
    }

private:
    std::string filePath_;
    std::ofstream outputStream_;
};

void init(const char * processor_name, void(*callback)(processor_session * ps, processor_context * ctx)) {
    const char * port_str = "9978";
    nifi_port port;
    port.port_id = (char *)port_str;
    const char * instance_str = "nifi";
    instance = create_instance(instance_str, &port);
    add_custom_processor(processor_name, callback);
    proc = create_processor(processor_name);
    make_dir("content_repository");
}

void teardown() {
    remove_directory("content_repository");
    free_flow_file_list(&ff_list);
    free_standalone_processor(proc);
    free_instance(instance);
}

class TailFileTestResourceManager {
public:
    TailFileTestResourceManager(const std::string& processor_name, void(*callback)(processor_session * ps, processor_context * ctx)) {
        init(processor_name.c_str(), callback);
    }

    ~TailFileTestResourceManager() {
        teardown();
    }
};

#endif /* NANOFI_TESTS_CTESTSBASE_H_ */
#endif

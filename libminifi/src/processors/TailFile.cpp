/**
 * @file TailFile.cpp
 * TailFile class implementation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>

#include <limits.h>
#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#endif
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <sstream>
#include <string>
#include <iostream>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "processors/TailFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property TailFile::FileName("File to Tail", "Fully-qualified filename of the file that should be tailed", "");
core::Property TailFile::StateFile("State File", "Specifies the file that should be used for storing state about"
                                   " what data has been ingested so that upon restart NiFi can resume from where it left off",
                                   "TailFileState");
core::Property TailFile::Delimiter("Input Delimiter", "Specifies the character that should be used for delimiting the data being tailed"
                                   "from the incoming file.",
                                   "");
core::Relationship TailFile::Success("success", "All files are routed to success");

void TailFile::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(FileName);
  properties.insert(StateFile);
  properties.insert(Delimiter);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void TailFile::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  std::string value;

  if (context->getProperty(Delimiter.getName(), value)) {
    delimiter_ = value;
  }
}

std::string TailFile::trimLeft(const std::string& s) {
  return org::apache::nifi::minifi::utils::StringUtils::trimLeft(s);
}

std::string TailFile::trimRight(const std::string& s) {
  return org::apache::nifi::minifi::utils::StringUtils::trimRight(s);
}

void TailFile::parseStateFileLine(char *buf) {
  char *line = buf;

  while ((line[0] == ' ') || (line[0] == '\t'))
    ++line;

  char first = line[0];
  if ((first == '\0') || (first == '#') || (first == '\r') || (first == '\n') || (first == '=')) {
    return;
  }

  char *equal = strchr(line, '=');
  if (equal == NULL) {
    return;
  }

  equal[0] = '\0';
  std::string key = line;

  equal++;
  while ((equal[0] == ' ') || (equal[0] == '\t'))
    ++equal;

  first = equal[0];
  if ((first == '\0') || (first == '\r') || (first == '\n')) {
    return;
  }

  std::string value = equal;
  key = trimRight(key);
  value = trimRight(value);

  if (key == "FILENAME")
    this->_currentTailFileName = value;
  if (key == "POSITION")
    this->_currentTailFilePosition = std::stoi(value);

  return;
}

void TailFile::recoverState() {
  std::ifstream file(_stateFile.c_str(), std::ifstream::in);
  if (!file.good()) {
    logger_->log_error("load state file failed %s", _stateFile);
    return;
  }
  char buf[BUFFER_SIZE];
  for (file.getline(buf, BUFFER_SIZE); file.good(); file.getline(buf, BUFFER_SIZE)) {
    parseStateFileLine(buf);
  }
}

void TailFile::storeState() {
  std::ofstream file(_stateFile.c_str());
  if (!file.is_open()) {
    logger_->log_error("store state file failed %s", _stateFile);
    return;
  }
  file << "FILENAME=" << this->_currentTailFileName << "\n";
  file << "POSITION=" << this->_currentTailFilePosition << "\n";
  file.close();
}

static bool sortTailMatchedFileItem(TailMatchedFileItem i, TailMatchedFileItem j) {
  return (i.modifiedTime < j.modifiedTime);
}
void TailFile::checkRollOver(const std::string &fileLocation, const std::string &fileName) {
  struct stat statbuf;
  std::vector<TailMatchedFileItem> matchedFiles;
  std::string fullPath = fileLocation + "/" + _currentTailFileName;

  if (stat(fullPath.c_str(), &statbuf) == 0) {
    if (statbuf.st_size > this->_currentTailFilePosition)
      // there are new input for the current tail file
      return;

    uint64_t modifiedTimeCurrentTailFile = ((uint64_t) (statbuf.st_mtime) * 1000);
    std::string pattern = fileName;
    std::size_t found = fileName.find_last_of(".");
    if (found != std::string::npos)
      pattern = fileName.substr(0, found);
#ifndef WIN32
    DIR *d;
    d = opendir(fileLocation.c_str());
    if (!d)
      return;
    while (1) {
      struct dirent *entry;
      entry = readdir(d);
      if (!entry)
        break;
      std::string d_name = entry->d_name;
      if (!(entry->d_type & DT_DIR)) {
        std::string fileName = d_name;
        std::string fileFullName = fileLocation + "/" + d_name;
        if (fileFullName.find(pattern) != std::string::npos && stat(fileFullName.c_str(), &statbuf) == 0) {
          if (((uint64_t) (statbuf.st_mtime) * 1000) >= modifiedTimeCurrentTailFile) {
            TailMatchedFileItem item;
            item.fileName = fileName;
            item.modifiedTime = ((uint64_t) (statbuf.st_mtime) * 1000);
            matchedFiles.push_back(item);
          }
        }
      }
    }
    closedir(d);
#else

    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;

    if ((hFind = FindFirstFile(fileLocation.c_str(), &FindFileData)) != INVALID_HANDLE_VALUE) {
      do {
        struct stat statbuf {};
        if (stat(FindFileData.cFileName, &statbuf) != 0) {
          logger_->log_warn("Failed to stat %s", FindFileData.cFileName);
          break;
        }

        std::string fileFullName = fileLocation + "/" + FindFileData.cFileName;

        if (fileFullName.find(pattern) != std::string::npos && stat(fileFullName.c_str(), &statbuf) == 0) {
          if (((uint64_t)(statbuf.st_mtime) * 1000) >= modifiedTimeCurrentTailFile) {
            TailMatchedFileItem item;
            item.fileName = fileName;
            item.modifiedTime = ((uint64_t)(statbuf.st_mtime) * 1000);
            matchedFiles.push_back(item);
          }
        }
      }while (FindNextFile(hFind, &FindFileData));
      FindClose(hFind);
    }
#endif

    // Sort the list based on modified time
    std::sort(matchedFiles.begin(), matchedFiles.end(), sortTailMatchedFileItem);
    for (std::vector<TailMatchedFileItem>::iterator it = matchedFiles.begin(); it != matchedFiles.end(); ++it) {
      TailMatchedFileItem item = *it;
      if (item.fileName == _currentTailFileName) {
        ++it;
        if (it != matchedFiles.end()) {
          TailMatchedFileItem nextItem = *it;
          logger_->log_info("TailFile File Roll Over from %s to %s", _currentTailFileName, nextItem.fileName);
          _currentTailFileName = nextItem.fileName;
          _currentTailFilePosition = 0;
          storeState();
        }
        break;
      }
    }
  } else {
    return;
  }
}

void TailFile::onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  std::lock_guard<std::mutex> tail_lock(tail_file_mutex_);
  std::string value;
  std::string fileLocation = "";
  std::string fileName = "";
  if (context->getProperty(FileName.getName(), value)) {
    std::size_t found = value.find_last_of("/\\");
    fileLocation = value.substr(0, found);
    fileName = value.substr(found + 1);
  }
  if (context->getProperty(StateFile.getName(), value)) {
    _stateFile = value + "." + getUUIDStr();
  }
  if (!this->_stateRecovered) {
    _stateRecovered = true;
    this->_currentTailFileName = fileName;
    this->_currentTailFilePosition = 0;
    // recover the state if we have not done so
    this->recoverState();
  }
  checkRollOver(fileLocation, fileName);
  std::string fullPath = fileLocation + "/" + _currentTailFileName;
  struct stat statbuf;

  logger_->log_debug("Tailing file %s", fullPath);
  if (stat(fullPath.c_str(), &statbuf) == 0) {
    if ((uint64_t) statbuf.st_size <= this->_currentTailFilePosition) {
      // there are no new input for the current tail file
      context->yield();
      return;
    }
    std::size_t found = _currentTailFileName.find_last_of(".");
    std::string baseName = _currentTailFileName.substr(0, found);
    std::string extension = _currentTailFileName.substr(found + 1);

    if (!delimiter_.empty()) {
      char delim = delimiter_.c_str()[0];
      if (delim == '\\') {
        if (delimiter_.size() > 1) {
          switch (delimiter_.c_str()[1]) {
            case 'r':
              delim = '\r';
              break;
            case 't':
              delim = '\t';
              break;
            case 'n':
              delim = '\n';
              break;
            case '\\':
              delim = '\\';
              break;
            default:
              // previous behavior
              break;
          }
        }
      }
      logger_->log_debug("Looking for delimiter 0x%X", delim);
      std::vector<std::shared_ptr<FlowFileRecord>> flowFiles;
      session->import(fullPath, flowFiles, true, this->_currentTailFilePosition, delim);
      logger_->log_info("%u flowfiles were received from TailFile input", flowFiles.size());

      for (auto ffr : flowFiles) {
        logger_->log_info("TailFile %s for %u bytes", _currentTailFileName, ffr->getSize());
        std::string logName = baseName + "." + std::to_string(_currentTailFilePosition) + "-" + std::to_string(_currentTailFilePosition + ffr->getSize()) + "." + extension;
        ffr->updateKeyedAttribute(PATH, fileLocation);
        ffr->addKeyedAttribute(ABSOLUTE_PATH, fullPath);
        ffr->updateKeyedAttribute(FILENAME, logName);
        session->transfer(ffr, Success);
        this->_currentTailFilePosition += ffr->getSize() + 1;
        storeState();
      }

    } else {
      std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->create());
      if (!flowFile)
        return;
      flowFile->updateKeyedAttribute(PATH, fileLocation);
      flowFile->addKeyedAttribute(ABSOLUTE_PATH, fullPath);
      session->import(fullPath, flowFile, true, this->_currentTailFilePosition);
      session->transfer(flowFile, Success);
      logger_->log_info("TailFile %s for %llu bytes", _currentTailFileName, flowFile->getSize());
      std::string logName = baseName + "." + std::to_string(_currentTailFilePosition) + "-" + std::to_string(_currentTailFilePosition + flowFile->getSize()) + "." + extension;
      flowFile->updateKeyedAttribute(FILENAME, logName);
      this->_currentTailFilePosition += flowFile->getSize();
      storeState();
    }

  } else {
    logger_->log_warn("Unable to stat file %s", fullPath);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

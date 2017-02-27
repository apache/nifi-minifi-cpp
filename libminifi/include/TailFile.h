/**
 * @file TailFile.h
 * TailFile class declaration
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
#ifndef __TAIL_FILE_H__
#define __TAIL_FILE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! TailFile Class
class TailFile : public Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  TailFile(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {
    logger_ = Logger::getLogger();
    _stateRecovered = false;
  }
  //! Destructor
  virtual ~TailFile() {
    storeState();
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Properties
  static Property FileName;
  static Property StateFile;
  //! Supported Relationships
  static Relationship Success;

 public:
  //! OnTrigger method, implemented by NiFi TailFile
  virtual void onTrigger(ProcessContext *context, ProcessSession *session);
  //! Initialize, over write by NiFi TailFile
  virtual void initialize(void);
  //! recoverState
  void recoverState();
  //! storeState
  void storeState();

 protected:

 private:
  //! Logger
  std::shared_ptr<Logger> logger_;
  std::string _fileLocation;
  //! Property Specified Tailed File Name
  std::string _fileName;
  //! File to save state
  std::string _stateFile;
  //! State related to the tailed file
  std::string _currentTailFileName;
  uint64_t _currentTailFilePosition;
  bool _stateRecovered;
  uint64_t _currentTailFileCreatedTime;
  //! Utils functions for parse state file
  std::string trimLeft(const std::string& s);
  std::string trimRight(const std::string& s);
  void parseStateFileLine(char *buf);
  void checkRollOver();

};

//! Matched File Item for Roll over check
typedef struct {
  std::string fileName;
  uint64_t modifiedTime;
} TailMatchedFileItem;

#endif

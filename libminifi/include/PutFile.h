/**
 * @file PutFile.h
 * PutFile class declaration
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
#ifndef __PUT_FILE_H__
#define __PUT_FILE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! PutFile Class
class PutFile : public Processor {
 public:

  static const std::string CONFLICT_RESOLUTION_STRATEGY_REPLACE;
  static const std::string CONFLICT_RESOLUTION_STRATEGY_IGNORE;
  static const std::string CONFLICT_RESOLUTION_STRATEGY_FAIL;

  //! Constructor
  /*!
   * Create a new processor
   */
  PutFile(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {
    logger_ = Logger::getLogger();
  }
  //! Destructor
  virtual ~PutFile() {
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Properties
  static Property Directory;
  static Property ConflictResolution;
  //! Supported Relationships
  static Relationship Success;
  static Relationship Failure;

  //! OnTrigger method, implemented by NiFi PutFile
  virtual void onTrigger(ProcessContext *context, ProcessSession *session);
  //! Initialize, over write by NiFi PutFile
  virtual void initialize(void);

  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(const std::string &tmpFile, const std::string &destFile);
    ~ReadCallback();
    virtual void process(std::ifstream *stream);
    bool commit();

   private:
    std::shared_ptr<Logger> logger_;
    std::ofstream _tmpFileOs;
    bool _writeSucceeded = false;
    std::string _tmpFile;
    std::string _destFile;
  };

 protected:

 private:
  //! Logger
  std::shared_ptr<Logger> logger_;

  bool putFile(ProcessSession *session, FlowFileRecord *flowFile,
               const std::string &tmpFile, const std::string &destFile);
};

#endif

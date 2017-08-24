/**
 * @file MergeContent.h
 * MergeContent class declaration
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
#ifndef __MERGE_CONTENT_H__
#define __MERGE_CONTENT_H__

#include "processors/BinFiles.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MERGE_STRATEGY_BIN_PACK "Bin-Packing Algorithm"
#define MERGE_STRATEGY_DEFRAGMENT "Defragment"
#define MERGE_FORMAT_TAR_VALUE "TAR"
#define MERGE_FORMAT_ZIP_VALUE "ZIP"
#define MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE "FlowFile Stream, v3"
#define MERGE_FORMAT_FLOWFILE_STREAM_V2_VALUE  "FlowFile Stream, v2"
#define MERGE_FORMAT_FLOWFILE_TAR_V1_VALUE  "FlowFile Tar, v1"
#define MERGE_FORMAT_CONCAT_VALUE "Binary Concatenation"
#define MERGE_FORMAT_AVRO_VALUE "Avro"
#define DELIMITER_STRATEGY_FILENAME "Filename"
#define DELIMITER_STRATEGY_TEXT "Text"

// MergeBin Class
class MergeBin {
public:
  virtual std::string getMergedContentType() = 0;
  // merge the flows in the bin
  virtual std::shared_ptr<core::FlowFile> merge(core::ProcessContext *context, core::ProcessSession *session,
      std::deque<std::shared_ptr<core::FlowFile>> &flows, std::string &header, std::string &footer, std::string &demarcator) = 0;
};

// BinaryConcatenationMerge Class
class BinaryConcatenationMerge : public MergeBin {
public:
  static const char *mimeType;
  std::string getMergedContentType() {
    return mimeType;
  }
  std::shared_ptr<core::FlowFile> merge(core::ProcessContext *context, core::ProcessSession *session,
          std::deque<std::shared_ptr<core::FlowFile>> &flows, std::string &header, std::string &footer, std::string &demarcator);
  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t size, std::shared_ptr<io::BaseStream> stream)
        : buffer_size_(size), stream_(stream) {
    }
    ~ReadCallback() {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      uint8_t buffer[buffer_size_];
      int64_t ret = 0;
      uint64_t read_size;
      ret = stream->read(buffer, buffer_size_);
      if (!stream)
        read_size = stream->getSize();
      else
        read_size = buffer_size_;
      ret = stream_->write(buffer, read_size);
      return ret;
    }
    uint64_t buffer_size_;
    std::shared_ptr<io::BaseStream> stream_;
  };
  // Nest Callback Class for write stream
  class WriteCallback: public OutputStreamCallback {
  public:
    WriteCallback(std::string &header, std::string &footer, std::string &demarcator, std::deque<std::shared_ptr<core::FlowFile>> &flows, core::ProcessSession *session) :
      header_(header), footer_(footer), demarcator_(demarcator), flows_(flows), session_(session) {
    }
    std::string &header_;
    std::string &footer_;
    std::string &demarcator_;
    std::deque<std::shared_ptr<core::FlowFile>> &flows_;
    core::ProcessSession *session_;
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t ret = 0;
      if (!header_.empty()) {
        int64_t len = stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(header_.data())), header_.size());
        if (len < 0)
          return len;
        ret += len;
      }
      bool isFirst = true;
      for (auto flow : flows_) {
        if (!isFirst && !demarcator_.empty()) {
          int64_t len = stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(demarcator_.data())), demarcator_.size());
          if (len < 0)
            return len;
          ret += len;
        }
        ReadCallback readCb(flow->getSize(), stream);
        session_->read(flow, &readCb);
        ret += flow->getSize();
        isFirst = false;
      }
      if (!footer_.empty()) {
        int64_t len = stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(footer_.data())), footer_.size());
        if (len < 0)
          return len;
        ret += len;
      }
      return ret;
    }
  };
};


// MergeContent Class
class MergeContent : public processors::BinFiles {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit MergeContent(std::string name, uuid_t uuid = NULL)
      : processors::BinFiles(name, uuid),
        logger_(logging::LoggerFactory<MergeContent>::getLogger()) {
    mergeStratgey_ = MERGE_STRATEGY_DEFRAGMENT;
    mergeFormat_ = MERGE_FORMAT_CONCAT_VALUE;
    delimiterStratgey_ = DELIMITER_STRATEGY_FILENAME;
    keepPath_ = false;
  }
  // Destructor
  virtual ~MergeContent() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "MergeContent";
  // Supported Properties
  static core::Property MergeStrategy;
  static core::Property MergeFormat;
  static core::Property CorrelationAttributeName;
  static core::Property DelimiterStratgey;
  static core::Property KeepPath;
  static core::Property Header;
  static core::Property Footer;
  static core::Property Demarcator;

  // Supported Relationships
  static core::Relationship Merge;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  // OnTrigger method, implemented by NiFi MergeContent
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);
  // Initialize, over write by NiFi MergeContent
  virtual void initialize(void);
  virtual bool processBin(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin);

 protected:
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  virtual std::string getGroupId(core::ProcessContext *context, std::shared_ptr<core::FlowFile> flow);
  // check whether the defragment bin is validate
  bool checkDefragment(std::unique_ptr<Bin> &bin);

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string mergeStratgey_;
  std::string mergeFormat_;
  std::string correlationAttributeName_;
  bool keepPath_;
  std::string delimiterStratgey_;
  std::string header_;
  std::string footer_;
  std::string demarcator_;
  std::string headerContent_;
  std::string footerContent_;
  std::string demarcatorContent_;
  // readContent
  std::string readContent(std::string path);
};

REGISTER_RESOURCE(MergeContent);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

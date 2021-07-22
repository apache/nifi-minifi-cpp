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
#pragma once

#include <deque>
#include <map>
#include <vector>
#include <memory>
#include <string>

#include "ArchiveCommon.h"
#include "BinFiles.h"
#include "archive_entry.h"
#include "archive.h"
#include "core/logging/LoggerConfiguration.h"
#include "serialization/FlowFileSerializer.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

namespace merge_content_options {

constexpr const char *MERGE_STRATEGY_BIN_PACK = "Bin-Packing Algorithm";
constexpr const char *MERGE_STRATEGY_DEFRAGMENT = "Defragment";
constexpr const char *MERGE_FORMAT_TAR_VALUE = "TAR";
constexpr const char *MERGE_FORMAT_ZIP_VALUE = "ZIP";
constexpr const char *MERGE_FORMAT_CONCAT_VALUE = "Binary Concatenation";
constexpr const char* MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE = "FlowFile Stream, v3";
constexpr const char *DELIMITER_STRATEGY_FILENAME = "Filename";
constexpr const char *DELIMITER_STRATEGY_TEXT = "Text";
constexpr const char *ATTRIBUTE_STRATEGY_KEEP_COMMON = "Keep Only Common Attributes";
constexpr const char *ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE = "Keep All Unique Attributes";

} /* namespace merge_content_options */

// MergeBin Class
class MergeBin {
 public:
  virtual ~MergeBin() = default;
  // merge the flows in the bin
  virtual void merge(core::ProcessContext *context, core::ProcessSession *session,
      std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &flowFile) = 0;
};

// BinaryConcatenationMerge Class
class BinaryConcatenationMerge : public MergeBin {
 public:
  BinaryConcatenationMerge(const std::string& header, const std::string& footer, const std::string& demarcator);

  void merge(core::ProcessContext *context, core::ProcessSession *session,
      std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &flowFile) override;
  // Nest Callback Class for write stream
  class WriteCallback: public OutputStreamCallback {
   public:
    WriteCallback(std::string &header, std::string &footer, std::string &demarcator,
        std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer) :
      header_(header), footer_(footer), demarcator_(demarcator), flows_(flows), serializer_(serializer) {
    }

    std::string &header_;
    std::string &footer_;
    std::string &demarcator_;
    std::deque<std::shared_ptr<core::FlowFile>> &flows_;
    FlowFileSerializer& serializer_;

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      size_t write_size_sum = 0;
      if (!header_.empty()) {
        const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(header_.data()), header_.size());
        if (io::isError(write_ret))
          return -1;
        write_size_sum += write_ret;
      }
      bool isFirst = true;
      for (const auto& flow : flows_) {
        if (!isFirst && !demarcator_.empty()) {
          const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(demarcator_.data()), demarcator_.size());
          if (io::isError(write_ret))
            return -1;
          write_size_sum += write_ret;
        }
        const auto len = serializer_.serialize(flow, stream);
        if (len < 0)
          return len;
        write_size_sum += gsl::narrow<size_t>(len);
        isFirst = false;
      }
      if (!footer_.empty()) {
        const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(footer_.data()), footer_.size());
        if (io::isError(write_ret))
          return -1;
        write_size_sum += write_ret;
      }
      return gsl::narrow<int64_t>(write_size_sum);
    }
  };

 private:
  std::string header_;
  std::string footer_;
  std::string demarcator_;
};


// Archive Class
class ArchiveMerge {
 public:
  class ArchiveWriter : public io::OutputStream {
   public:
    ArchiveWriter(struct archive *arch, struct archive_entry *entry) : arch_(arch), entry_(entry) {}
    size_t write(const uint8_t* data, size_t size) override {
      if (!header_emitted_) {
        if (archive_write_header(arch_, entry_) != ARCHIVE_OK) {
          return io::STREAM_ERROR;
        }
        header_emitted_ = true;
      }
      size_t totalWrote = 0;
      size_t remaining = size;
      while (remaining > 0) {
        const auto ret = archive_write_data(arch_, data + totalWrote, remaining);
        if (ret < 0) {
          return io::STREAM_ERROR;
        }
        const auto zret = gsl::narrow<size_t>(ret);
        if (zret == 0) {
          break;
        }
        totalWrote += zret;
        remaining -= zret;
      }
      return totalWrote;
    }

   private:
    struct archive *arch_;
    struct archive_entry *entry_;
    bool header_emitted_{false};
  };
  // Nest Callback Class for write stream
  class WriteCallback: public OutputStreamCallback {
   public:
    WriteCallback(std::string merge_type, std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer)
        : merge_type_(merge_type),
          flows_(flows),
          logger_(logging::LoggerFactory<ArchiveMerge>::getLogger()),
          serializer_(serializer) {
      size_ = 0;
      stream_ = nullptr;
    }
    ~WriteCallback() override = default;

    std::string merge_type_;
    std::deque<std::shared_ptr<core::FlowFile>> &flows_;
    std::shared_ptr<io::BaseStream> stream_;
    size_t size_;
    std::shared_ptr<logging::Logger> logger_;
    FlowFileSerializer& serializer_;

    static la_ssize_t archive_write(struct archive* /*arch*/, void *context, const void *buff, size_t size) {
      WriteCallback *callback = reinterpret_cast<WriteCallback *>(context);
      uint8_t* data = reinterpret_cast<uint8_t*>(const_cast<void*>(buff));
      la_ssize_t totalWrote = 0;
      size_t remaining = size;
      while (remaining > 0) {
        const auto ret = callback->stream_->write(data + totalWrote, remaining);
        if (io::isError(ret)) {
          // libarchive expects us to return -1 on error
          return -1;
        }
        if (ret == 0) {
          break;
        }
        callback->size_ += ret;
        totalWrote += static_cast<la_ssize_t>(ret);
        remaining -= ret;
      }
      return totalWrote;
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      struct archive *arch;

      arch = archive_write_new();
      if (merge_type_ == merge_content_options::MERGE_FORMAT_TAR_VALUE) {
        archive_write_set_format_pax_restricted(arch);  // tar format
      }
      if (merge_type_ == merge_content_options::MERGE_FORMAT_ZIP_VALUE) {
        archive_write_set_format_zip(arch);  // zip format
      }
      archive_write_set_bytes_per_block(arch, 0);
      archive_write_add_filter_none(arch);
      stream_ = stream;
      archive_write_open(arch, this, NULL, archive_write, NULL);

      for (auto flow : flows_) {
        struct archive_entry *entry = archive_entry_new();
        std::string fileName;
        flow->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
        archive_entry_set_pathname(entry, fileName.c_str());
        archive_entry_set_size(entry, flow->getSize());
        archive_entry_set_mode(entry, S_IFREG | 0755);
        if (merge_type_ == merge_content_options::MERGE_FORMAT_TAR_VALUE) {
          std::string perm;
          int permInt;
          if (flow->getAttribute(BinFiles::TAR_PERMISSIONS_ATTRIBUTE, perm)) {
            try {
              permInt = std::stoi(perm);
              logger_->log_debug("Merge Tar File %s permission %s", fileName, perm);
              archive_entry_set_perm(entry, (mode_t) permInt);
            } catch (...) {
            }
          }
        }
        const auto ret = serializer_.serialize(flow, std::make_shared<ArchiveWriter>(arch, entry));
        if (ret < 0) {
          return ret;
        }
        archive_entry_free(entry);
      }

      archive_write_close(arch);
      archive_write_free(arch);
      return size_;
    }
  };
};

// TarMerge Class
class TarMerge: public ArchiveMerge, public MergeBin {
 public:
  void merge(core::ProcessContext *context, core::ProcessSession *session, std::deque<std::shared_ptr<core::FlowFile>> &flows,
             FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &merge_flow) override;
};

// ZipMerge Class
class ZipMerge: public ArchiveMerge, public MergeBin {
 public:
  void merge(core::ProcessContext *context, core::ProcessSession *session, std::deque<std::shared_ptr<core::FlowFile>> &flows,
             FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &merge_flow) override;
};

class AttributeMerger {
 public:
  explicit AttributeMerger(std::deque<std::shared_ptr<org::apache::nifi::minifi::core::FlowFile>> &flows)
    : flows_(flows) {}
  void mergeAttributes(core::ProcessSession *session, const std::shared_ptr<core::FlowFile> &merge_flow);
  virtual ~AttributeMerger() = default;

 protected:
  std::map<std::string, std::string> getMergedAttributes();
  virtual void processFlowFile(const std::shared_ptr<core::FlowFile> &flow_file, std::map<std::string, std::string> &merged_attributes) = 0;

  const std::deque<std::shared_ptr<core::FlowFile>> &flows_;
};

class KeepOnlyCommonAttributesMerger: public AttributeMerger {
 public:
  explicit KeepOnlyCommonAttributesMerger(std::deque<std::shared_ptr<org::apache::nifi::minifi::core::FlowFile>> &flows)
    : AttributeMerger(flows) {}

 protected:
  void processFlowFile(const std::shared_ptr<core::FlowFile> &flow_file, std::map<std::string, std::string> &merged_attributes) override;
};

class KeepAllUniqueAttributesMerger: public AttributeMerger {
 public:
  explicit KeepAllUniqueAttributesMerger(std::deque<std::shared_ptr<org::apache::nifi::minifi::core::FlowFile>> &flows)
    : AttributeMerger(flows) {}

 protected:
  void processFlowFile(const std::shared_ptr<core::FlowFile> &flow_file, std::map<std::string, std::string> &merged_attributes) override;

 private:
  std::vector<std::string> removed_attributes_;
};

// MergeContent Class
class MergeContent : public processors::BinFiles {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit MergeContent(const std::string& name, const utils::Identifier& uuid = {})
      : processors::BinFiles(name, uuid),
        logger_(logging::LoggerFactory<MergeContent>::getLogger()) {
    mergeStrategy_ = merge_content_options::MERGE_STRATEGY_DEFRAGMENT;
    mergeFormat_ = merge_content_options::MERGE_FORMAT_CONCAT_VALUE;
    delimiterStrategy_ = merge_content_options::DELIMITER_STRATEGY_FILENAME;
    keepPath_ = false;
    attributeStrategy_ = merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON;
  }
  // Destructor
  ~MergeContent() override = default;
  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "MergeContent";
  // Supported Properties
  EXTENSIONAPI static core::Property MergeStrategy;
  EXTENSIONAPI static core::Property MergeFormat;
  EXTENSIONAPI static core::Property CorrelationAttributeName;
  EXTENSIONAPI static core::Property DelimiterStrategy;
  EXTENSIONAPI static core::Property KeepPath;
  EXTENSIONAPI static core::Property Header;
  EXTENSIONAPI static core::Property Footer;
  EXTENSIONAPI static core::Property Demarcator;
  EXTENSIONAPI static core::Property AttributeStrategy;

  // Supported Relationships
  EXTENSIONAPI static core::Relationship Merge;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  // OnTrigger method, implemented by NiFi MergeContent
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  // Initialize, over write by NiFi MergeContent
  void initialize() override;
  bool processBin(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) override;

 protected:
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  std::string getGroupId(core::ProcessContext *context, std::shared_ptr<core::FlowFile> flow) override;
  // check whether the defragment bin is validate
  bool checkDefragment(std::unique_ptr<Bin> &bin);

 private:
  void validatePropertyOptions();

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::shared_ptr<logging::Logger> logger_;
  std::string mergeStrategy_;
  std::string mergeFormat_;
  std::string correlationAttributeName_;
  bool keepPath_;
  std::string delimiterStrategy_;
  std::string header_;
  std::string footer_;
  std::string demarcator_;
  std::string headerContent_;
  std::string footerContent_;
  std::string demarcatorContent_;
  std::string attributeStrategy_;
  // readContent
  std::string readContent(std::string path);
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

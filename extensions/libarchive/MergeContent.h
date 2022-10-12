/**
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
#include <utility>
#include <vector>
#include <memory>
#include <string>

#include "ArchiveCommon.h"
#include "BinFiles.h"
#include "archive_entry.h"
#include "archive.h"
#include "core/logging/LoggerConfiguration.h"
#include "serialization/FlowFileSerializer.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::processors {

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

class MergeBin {
 public:
  virtual ~MergeBin() = default;
  // merge the flows in the bin
  virtual void merge(core::ProcessContext *context, core::ProcessSession *session,
      std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &flowFile) = 0;
};

class BinaryConcatenationMerge : public MergeBin {
 public:
  BinaryConcatenationMerge(std::string header, std::string footer, std::string demarcator);

  void merge(core::ProcessContext* context, core::ProcessSession *session,
    std::deque<std::shared_ptr<core::FlowFile>>& flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile>& merge_flow) override;
  // Nest Callback Class for write stream
  class WriteCallback {
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

    int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) const {
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
  class WriteCallback {
   public:
    WriteCallback(std::string merge_type, std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer)
        : merge_type_(std::move(merge_type)),
          flows_(flows),
          serializer_(serializer) {
      size_ = 0;
      stream_ = nullptr;
    }

    std::string merge_type_;
    std::deque<std::shared_ptr<core::FlowFile>> &flows_;
    std::shared_ptr<io::OutputStream> stream_;
    size_t size_;
    std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ArchiveMerge>::getLogger();
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

    int64_t operator()(const std::shared_ptr<io::OutputStream>& stream) {
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

class TarMerge: public ArchiveMerge, public MergeBin {
 public:
  void merge(core::ProcessContext *context, core::ProcessSession *session, std::deque<std::shared_ptr<core::FlowFile>> &flows,
             FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &merge_flow) override;
};

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

class MergeContent : public processors::BinFiles {
 public:
  explicit MergeContent(const std::string& name, const utils::Identifier& uuid = {})
      : processors::BinFiles(name, uuid) {
    mergeStrategy_ = merge_content_options::MERGE_STRATEGY_DEFRAGMENT;
    mergeFormat_ = merge_content_options::MERGE_FORMAT_CONCAT_VALUE;
    delimiterStrategy_ = merge_content_options::DELIMITER_STRATEGY_FILENAME;
    keepPath_ = false;
    attributeStrategy_ = merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON;
  }
  ~MergeContent() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Merges a Group of FlowFiles together based on a user-defined strategy and packages them into a single FlowFile. "
      "MergeContent should be configured with only one incoming connection as it won't create grouped Flow Files."
      "This processor updates the mime.type attribute as appropriate.";

  EXTENSIONAPI static const core::Property MergeStrategy;
  EXTENSIONAPI static const core::Property MergeFormat;
  EXTENSIONAPI static const core::Property CorrelationAttributeName;
  EXTENSIONAPI static const core::Property DelimiterStrategy;
  EXTENSIONAPI static const core::Property KeepPath;
  EXTENSIONAPI static const core::Property Header;
  EXTENSIONAPI static const core::Property Footer;
  EXTENSIONAPI static const core::Property Demarcator;
  EXTENSIONAPI static const core::Property AttributeStrategy;
  static auto properties() {
    return utils::array_cat(BinFiles::properties(), std::array{
      MergeStrategy,
      MergeFormat,
      CorrelationAttributeName,
      DelimiterStrategy,
      KeepPath,
      Header,
      Footer,
      Demarcator,
      AttributeStrategy
    });
  }

  EXTENSIONAPI static const core::Relationship Merge;
  static auto relationships() {
    return utils::array_cat(BinFiles::relationships(), std::array{Merge});
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;
  bool processBin(core::ProcessContext *context, core::ProcessSession *session, std::unique_ptr<Bin> &bin) override;

 protected:
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  std::string getGroupId(core::ProcessContext *context, std::shared_ptr<core::FlowFile> flow) override;
  // check whether the defragment bin is validate
  static bool checkDefragment(std::unique_ptr<Bin> &bin);

 private:
  void validatePropertyOptions();

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<MergeContent>::getLogger();
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
  static std::string readContent(const std::string& path);
};

}  // namespace org::apache::nifi::minifi::processors

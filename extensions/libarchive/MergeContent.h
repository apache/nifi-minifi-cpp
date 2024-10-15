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
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "serialization/FlowFileSerializer.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"
#include "utils/Export.h"
#include "io/Stream.h"

namespace org::apache::nifi::minifi::processors {

namespace merge_content_options {

inline constexpr std::string_view MERGE_STRATEGY_BIN_PACK = "Bin-Packing Algorithm";
inline constexpr std::string_view MERGE_STRATEGY_DEFRAGMENT = "Defragment";
inline constexpr std::string_view MERGE_FORMAT_TAR_VALUE = "TAR";
inline constexpr std::string_view MERGE_FORMAT_ZIP_VALUE = "ZIP";
inline constexpr std::string_view MERGE_FORMAT_CONCAT_VALUE = "Binary Concatenation";
inline constexpr std::string_view MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE = "FlowFile Stream, v3";
inline constexpr std::string_view DELIMITER_STRATEGY_FILENAME = "Filename";
inline constexpr std::string_view DELIMITER_STRATEGY_TEXT = "Text";
inline constexpr std::string_view ATTRIBUTE_STRATEGY_KEEP_COMMON = "Keep Only Common Attributes";
inline constexpr std::string_view ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE = "Keep All Unique Attributes";

}  // namespace merge_content_options

class MergeBin {
 public:
  virtual ~MergeBin() = default;
  // merge the flows in the bin
  virtual void merge(core::ProcessSession &session,
      std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &flowFile) = 0;
};

class BinaryConcatenationMerge : public MergeBin {
 public:
  BinaryConcatenationMerge(std::string header, std::string footer, std::string demarcator);

  void merge(core::ProcessSession &session,
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
  class ArchiveWriter : public io::StreamImpl, public io::OutputStream {
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
    WriteCallback(std::string_view merge_type, std::deque<std::shared_ptr<core::FlowFile>> &flows, FlowFileSerializer& serializer)
        : merge_type_(merge_type),
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
      auto* callback = reinterpret_cast<WriteCallback *>(context);
      auto* data = reinterpret_cast<uint8_t*>(const_cast<void*>(buff));
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
      archive_write_open(arch, this, nullptr, archive_write, nullptr);

      for (const auto& flow : flows_) {
        struct archive_entry *entry = archive_entry_new();
        std::string fileName;
        flow->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
        archive_entry_set_pathname(entry, fileName.c_str());
        archive_entry_set_size(entry, gsl::narrow<la_int64_t>(flow->getSize()));
        archive_entry_set_mode(entry, S_IFREG | 0755);
        if (merge_type_ == merge_content_options::MERGE_FORMAT_TAR_VALUE) {
          std::string perm;
          int permInt;
          if (flow->getAttribute(BinFiles::TAR_PERMISSIONS_ATTRIBUTE, perm)) {
            try {
              permInt = std::stoi(perm);
              logger_->log_debug("Merge Tar File {} permission {}", fileName, perm);
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
      return gsl::narrow<int64_t>(size_);
    }
  };
};

class TarMerge: public ArchiveMerge, public MergeBin {
 public:
  void merge(core::ProcessSession &session, std::deque<std::shared_ptr<core::FlowFile>> &flows,
             FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &merge_flow) override;
};

class ZipMerge: public ArchiveMerge, public MergeBin {
 public:
  void merge(core::ProcessSession &session, std::deque<std::shared_ptr<core::FlowFile>> &flows,
             FlowFileSerializer& serializer, const std::shared_ptr<core::FlowFile> &merge_flow) override;
};

class AttributeMerger {
 public:
  explicit AttributeMerger(std::deque<std::shared_ptr<org::apache::nifi::minifi::core::FlowFile>> &flows)
    : flows_(flows) {}
  void mergeAttributes(core::ProcessSession &session, core::FlowFile& merge_flow);
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

/**
 * A processor that merges multiple correlated flow files to a single flow file
 *
 * Concepts:
 * - Batch size: represents the maximum number of flow files to be processed from the incoming relationship
 * - Bin (or bundle): represents a set of flow files that belong together defined by the processor properties. Correlated flow files are defined by the CorrelationAttributeName property which
 *                    defines the attribute that provides the groupid for the bin the flow file belongs to
 * - Ready bin: when a bin reaches a limit defined by the maximum age or the maximum size, the bin becomes ready, and ready bins can be merged
 * - Group: a set of bins with the same groupid. In case a bin cannot accept a new flow files (e.g. it would go above its size limit), a new bin is created with this new flow file and added
 *          to the same group of bins
 */
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

  EXTENSIONAPI static constexpr auto MergeStrategy = core::PropertyDefinitionBuilder<2>::createProperty("Merge Strategy")
      .withDescription("Defragment or Bin-Packing Algorithm")
      .withAllowedValues({merge_content_options::MERGE_STRATEGY_DEFRAGMENT, merge_content_options::MERGE_STRATEGY_BIN_PACK})
      .withDefaultValue(merge_content_options::MERGE_STRATEGY_BIN_PACK)
      .build();
  EXTENSIONAPI static constexpr auto MergeFormat = core::PropertyDefinitionBuilder<4>::createProperty("Merge Format")
      .withDescription("Merge Format")
      .withAllowedValues({
        merge_content_options::MERGE_FORMAT_CONCAT_VALUE,
        merge_content_options::MERGE_FORMAT_TAR_VALUE,
        merge_content_options::MERGE_FORMAT_ZIP_VALUE,
        merge_content_options::MERGE_FORMAT_FLOWFILE_STREAM_V3_VALUE})
      .withDefaultValue(merge_content_options::MERGE_FORMAT_CONCAT_VALUE)
      .build();
  EXTENSIONAPI static constexpr auto CorrelationAttributeName = core::PropertyDefinitionBuilder<>::createProperty("Correlation Attribute Name")
      .withDescription("Correlation Attribute Name")
      .build();
  EXTENSIONAPI static constexpr auto DelimiterStrategy = core::PropertyDefinitionBuilder<2>::createProperty("Delimiter Strategy")
      .withDescription("Determines if Header, Footer, and Demarcator should point to files")
      .withAllowedValues({merge_content_options::DELIMITER_STRATEGY_FILENAME, merge_content_options::DELIMITER_STRATEGY_TEXT})
      .withDefaultValue(merge_content_options::DELIMITER_STRATEGY_FILENAME)
      .build();
  EXTENSIONAPI static constexpr auto Header = core::PropertyDefinitionBuilder<>::createProperty("Header File")
      .withDescription("Filename specifying the header to use")
      .build();
  EXTENSIONAPI static constexpr auto Footer = core::PropertyDefinitionBuilder<>::createProperty("Footer File")
      .withDescription("Filename specifying the footer to use")
      .build();
  EXTENSIONAPI static constexpr auto Demarcator = core::PropertyDefinitionBuilder<>::createProperty("Demarcator File")
      .withDescription("Filename specifying the demarcator to use")
      .build();
  EXTENSIONAPI static constexpr auto KeepPath = core::PropertyDefinitionBuilder<>::createProperty("Keep Path")
      .withDescription("If using the Zip or Tar Merge Format, specifies whether or not the FlowFiles' paths should be included in their entry")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto AttributeStrategy = core::PropertyDefinitionBuilder<2>::createProperty("Attribute Strategy")
      .withDescription("Determines which FlowFile attributes should be added to the bundle. If 'Keep All Unique Attributes' is selected, "
          "any attribute on any FlowFile that gets bundled will be kept unless its value conflicts with the value from another FlowFile "
          "(in which case neither, or none, of the conflicting attributes will be kept). If 'Keep Only Common Attributes' is selected, "
          "only the attributes that exist on all FlowFiles in the bundle, with the same value, will be preserved.")
      .withAllowedValues({merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON, merge_content_options::ATTRIBUTE_STRATEGY_KEEP_ALL_UNIQUE})
      .withDefaultValue(merge_content_options::ATTRIBUTE_STRATEGY_KEEP_COMMON)
      .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(BinFiles::Properties, std::to_array<core::PropertyReference>({
      MergeStrategy,
      MergeFormat,
      CorrelationAttributeName,
      DelimiterStrategy,
      KeepPath,
      Header,
      Footer,
      Demarcator,
      AttributeStrategy
  }));


  EXTENSIONAPI static constexpr auto Merge = core::RelationshipDefinition{"merged", "The FlowFile containing the merged content"};
  EXTENSIONAPI static constexpr auto Relationships = utils::array_cat(BinFiles::Relationships, std::array{Merge});

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  bool processBin(core::ProcessSession &session, std::unique_ptr<Bin> &bin) override;

 protected:
  // Returns a group ID representing a bin. This allows flow files to be binned into like groups
  std::string getGroupId(const std::shared_ptr<core::FlowFile>& flow) override;
  // check whether the defragment bin is validate
  static bool checkDefragment(std::unique_ptr<Bin> &bin);

 private:
  void validatePropertyOptions();

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<MergeContent>::getLogger(uuid_);
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

/**
 * @file CompressContent.h
 * CompressContent class declaration
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

#include <cinttypes>
#include <vector>
#include <utility>
#include <memory>
#include <map>
#include <string>

#include "archive_entry.h"
#include "archive.h"

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/ZlibStream.h"
#include "utils/Enum.h"
#include "utils/gsl.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// CompressContent Class
class CompressContent : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit CompressContent(const std::string& name, const utils::Identifier& uuid = {})
    : core::Processor(name, uuid)
    , logger_(logging::LoggerFactory<CompressContent>::getLogger())
    , updateFileName_(false)
    , encapsulateInTar_(false) {
  }
  // Destructor
  ~CompressContent() override = default;
  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "CompressContent";
  // Supported Properties
  EXTENSIONAPI static core::Property CompressMode;
  EXTENSIONAPI static core::Property CompressLevel;
  EXTENSIONAPI static core::Property CompressFormat;
  EXTENSIONAPI static core::Property UpdateFileName;
  EXTENSIONAPI static core::Property EncapsulateInTar;
  EXTENSIONAPI static core::Property BatchSize;

  // Supported Relationships
  EXTENSIONAPI static core::Relationship Failure;
  EXTENSIONAPI static core::Relationship Success;

  static const std::string TAR_EXT;

  SMART_ENUM(CompressionMode,
    (Compress, "compress"),
    (Decompress, "decompress")
  )

  SMART_ENUM(CompressionFormat,
    (GZIP, "gzip"),
    (LZMA, "lzma"),
    (XZ_LZMA2, "xz-lzma2"),
    (BZIP2, "bzip2")
  )

  SMART_ENUM_EXTEND(ExtendedCompressionFormat, CompressionFormat, (GZIP, LZMA, XZ_LZMA2, BZIP2),
    (USE_MIME_TYPE, "use mime.type attribute")
  )

 public:
  // Nest Callback Class for read stream from flow for compress
  class ReadCallbackCompress: public InputStreamCallback {
   public:
    ReadCallbackCompress(std::shared_ptr<core::FlowFile> &flow, struct archive *arch, struct archive_entry *entry) :
        flow_(flow), arch_(arch), entry_(entry), status_(0), logger_(logging::LoggerFactory<CompressContent>::getLogger()) {
    }
    ~ReadCallbackCompress() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      uint8_t buffer[4096U];
      int64_t ret = 0;
      uint64_t read_size = 0;

      ret = archive_write_header(arch_, entry_);
      if (ret != ARCHIVE_OK) {
        logger_->log_error("Compress Content archive error %s", archive_error_string(arch_));
        status_ = -1;
        return -1;
      }
      while (read_size < flow_->getSize()) {
        const auto readret = stream->read(buffer, sizeof(buffer));
        if (io::isError(readret)) {
          status_ = -1;
          return -1;
        }
        if (readret > 0) {
          ret = archive_write_data(arch_, buffer, readret);
          if (ret < 0) {
            logger_->log_error("Compress Content archive error %s", archive_error_string(arch_));
            status_ = -1;
            return -1;
          }
          read_size += gsl::narrow<uint64_t>(ret);
        } else {
          break;
        }
      }
      return gsl::narrow<int64_t>(read_size);
    }
    std::shared_ptr<core::FlowFile> flow_;
    struct archive *arch_;
    struct archive_entry *entry_;
    int status_;
    std::shared_ptr<logging::Logger> logger_;
  };
  // Nest Callback Class for read stream from flow for decompress
  struct ReadCallbackDecompress : InputStreamCallback {
    explicit ReadCallbackDecompress(std::shared_ptr<core::FlowFile> flow) :
        flow_file(std::move(flow)) {
    }
    ~ReadCallbackDecompress() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      stream->seek(offset);
      const auto readRet = stream->read(buffer, sizeof(buffer));
      stream_read_result = readRet;
      if (!io::isError(readRet)) {
        offset += readRet;
      }
      return gsl::narrow<int64_t>(readRet);
    }
    size_t stream_read_result = 0;  // read size or error code, to be checked with io::isError
    uint8_t buffer[8192] = {0};
    size_t offset = 0;
    std::shared_ptr<core::FlowFile> flow_file;
  };
  // Nest Callback Class for write stream
  class WriteCallback: public OutputStreamCallback {
   public:
    WriteCallback(CompressionMode compress_mode, int compress_level, CompressionFormat compress_format,
        const std::shared_ptr<core::FlowFile> &flow, const std::shared_ptr<core::ProcessSession> &session) :
        compress_mode_(compress_mode), compress_level_(compress_level), compress_format_(compress_format),
        flow_(flow), session_(session),
        logger_(logging::LoggerFactory<CompressContent>::getLogger()),
        readDecompressCb_(flow) {
      size_ = 0;
      stream_ = nullptr;
      status_ = 0;
    }
    ~WriteCallback() = default;

    CompressionMode compress_mode_;
    int compress_level_;
    CompressionFormat compress_format_;
    std::shared_ptr<core::FlowFile> flow_;
    std::shared_ptr<core::ProcessSession> session_;
    std::shared_ptr<io::BaseStream> stream_;
    int64_t size_;
    std::shared_ptr<logging::Logger> logger_;
    CompressContent::ReadCallbackDecompress readDecompressCb_;
    int status_;

    static la_ssize_t archive_write(struct archive* /*arch*/, void *context, const void *buff, size_t size) {
      auto* const callback = static_cast<WriteCallback*>(context);
      const auto ret = callback->stream_->write(reinterpret_cast<const uint8_t*>(buff), size);
      if (!io::isError(ret)) callback->size_ += gsl::narrow<int64_t>(ret);
      return io::isError(ret) ? -1 : gsl::narrow<la_ssize_t>(ret);
    }

    static la_ssize_t archive_read(struct archive* archive, void *context, const void **buff) {
      auto *callback = reinterpret_cast<WriteCallback *>(context);
      callback->session_->read(callback->flow_, &callback->readDecompressCb_);
      *buff = callback->readDecompressCb_.buffer;
      if (io::isError(callback->readDecompressCb_.stream_read_result)) {
        archive_set_error(archive, EIO, "Error reading flowfile");
        return -1;
      }
      return gsl::narrow<la_ssize_t>(callback->readDecompressCb_.stream_read_result);
    }

    static la_int64_t archive_skip(struct archive* /*a*/, void* /*client_data*/, la_int64_t /*request*/) {
      return 0;
    }

    void archive_write_log_error_cleanup(struct archive *arch) {
      logger_->log_error("Compress Content archive write error %s", archive_error_string(arch));
      status_ = -1;
      archive_write_free(arch);
    }

    void archive_read_log_error_cleanup(struct archive *arch) {
      logger_->log_error("Compress Content archive read error %s", archive_error_string(arch));
      status_ = -1;
      archive_read_free(arch);
    }

    int64_t process(const std::shared_ptr<io::BaseStream>& stream) {
      struct archive *arch;
      int r;

      if (compress_mode_ == CompressionMode::Compress) {
        arch = archive_write_new();
        if (!arch) {
          status_ = -1;
          return -1;
        }
        r = archive_write_set_format_ustar(arch);
        if (r != ARCHIVE_OK) {
          archive_write_log_error_cleanup(arch);
          return -1;
        }
        if (compress_format_ == CompressionFormat::GZIP) {
          r = archive_write_add_filter_gzip(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
          std::string option;
          option = "gzip:compression-level=" + std::to_string(compress_level_);
          r = archive_write_set_options(arch, option.c_str());
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == CompressionFormat::BZIP2) {
          r = archive_write_add_filter_bzip2(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == CompressionFormat::LZMA) {
          r = archive_write_add_filter_lzma(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == CompressionFormat::XZ_LZMA2) {
          r = archive_write_add_filter_xz(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else {
            archive_write_log_error_cleanup(arch);
            return -1;
        }
        r = archive_write_set_bytes_per_block(arch, 0);
        if (r != ARCHIVE_OK) {
          archive_write_log_error_cleanup(arch);
          return -1;
        }
        this->stream_ = stream;
        r = archive_write_open(arch, this, NULL, archive_write, NULL);
        if (r != ARCHIVE_OK) {
          archive_write_log_error_cleanup(arch);
          return -1;
        }
        struct archive_entry *entry = archive_entry_new();
        if (!entry) {
          archive_write_log_error_cleanup(arch);
          return -1;
        }
        std::string fileName;
        flow_->getAttribute(core::SpecialFlowAttribute::FILENAME, fileName);
        archive_entry_set_pathname(entry, fileName.c_str());
        archive_entry_set_size(entry, flow_->getSize());
        archive_entry_set_mode(entry, S_IFREG | 0755);
        ReadCallbackCompress readCb(flow_, arch, entry);
        session_->read(flow_, &readCb);
        if (readCb.status_ < 0) {
          archive_entry_free(entry);
          archive_write_log_error_cleanup(arch);
          status_ = -1;
          return -1;
        }
        archive_entry_free(entry);
        archive_write_close(arch);
        archive_write_free(arch);
        return size_;
      } else {
        arch = archive_read_new();
        if (!arch) {
          status_ = -1;
          return -1;
        }
        r = archive_read_support_format_all(arch);
        if (r != ARCHIVE_OK) {
          archive_read_log_error_cleanup(arch);
          return -1;
        }
        r = archive_read_support_filter_all(arch);
        if (r != ARCHIVE_OK) {
          archive_read_log_error_cleanup(arch);
          return -1;
        }
        this->stream_ = stream;
        r = archive_read_open2(arch, this, NULL, archive_read, archive_skip, NULL);
        if (r != ARCHIVE_OK) {
          archive_read_log_error_cleanup(arch);
          return -1;
        }
        struct archive_entry *entry;
        if (archive_read_next_header(arch, &entry) != ARCHIVE_OK) {
          archive_read_log_error_cleanup(arch);
          return -1;
        }
        int64_t entry_size = archive_entry_size(entry);
        logger_->log_debug("Decompress Content archive entry size %" PRId64, entry_size);
        size_ = 0;
        while (size_ < entry_size) {
          char buffer[8192];
          const auto read_result = archive_read_data(arch, buffer, sizeof(buffer));
          if (read_result < 0) {
            archive_read_log_error_cleanup(arch);
            return -1;
          }
          if (read_result == 0)
            break;
          size_ += read_result;
          const auto write_result = stream_->write(reinterpret_cast<uint8_t*>(buffer), gsl::narrow<size_t>(read_result));
          if (io::isError(write_result)) {
            archive_read_log_error_cleanup(arch);
            return -1;
          }
        }
        archive_read_close(arch);
        archive_read_free(arch);
        return size_;
      }
    }
  };

  class GzipWriteCallback : public OutputStreamCallback {
   public:
    GzipWriteCallback(CompressionMode compress_mode, int compress_level, std::shared_ptr<core::FlowFile> flow, std::shared_ptr<core::ProcessSession> session)
      : logger_(logging::LoggerFactory<CompressContent>::getLogger())
      , compress_mode_(std::move(compress_mode))
      , compress_level_(compress_level)
      , flow_(std::move(flow))
      , session_(std::move(session)) {
    }

    std::shared_ptr<logging::Logger> logger_;
    CompressionMode compress_mode_;
    int compress_level_;
    std::shared_ptr<core::FlowFile> flow_;
    std::shared_ptr<core::ProcessSession> session_;
    bool success_{false};

    int64_t process(const std::shared_ptr<io::BaseStream>& outputStream) override {
      class ReadCallback : public InputStreamCallback {
       public:
        ReadCallback(GzipWriteCallback& writer, std::shared_ptr<io::OutputStream> outputStream)
          : writer_(writer)
          , outputStream_(std::move(outputStream)) {
        }

        int64_t process(const std::shared_ptr<io::BaseStream>& inputStream) override {
          std::vector<uint8_t> buffer(16 * 1024U);
          size_t read_size = 0;
          while (read_size < writer_.flow_->getSize()) {
            const auto ret = inputStream->read(buffer.data(), buffer.size());
            if (io::isError(ret)) {
              return -1;
            } else if (ret == 0) {
              break;
            } else {
              const auto writeret = outputStream_->write(buffer.data(), ret);
              if (io::isError(writeret) || gsl::narrow<size_t>(writeret) != ret) {
                return -1;
              }
              read_size += ret;
            }
          }
          outputStream_->close();
          return gsl::narrow<int64_t>(read_size);
        }

        GzipWriteCallback& writer_;
        std::shared_ptr<io::OutputStream> outputStream_;
      };

      std::shared_ptr<io::ZlibBaseStream> filterStream;
      if (compress_mode_ == CompressionMode::Compress) {
        filterStream = std::make_shared<io::ZlibCompressStream>(gsl::make_not_null(outputStream.get()), io::ZlibCompressionFormat::GZIP, compress_level_);
      } else {
        filterStream = std::make_shared<io::ZlibDecompressStream>(gsl::make_not_null(outputStream.get()), io::ZlibCompressionFormat::GZIP);
      }
      ReadCallback readCb(*this, filterStream);
      session_->read(flow_, &readCb);

      success_ = filterStream->isFinished();

      return gsl::narrow<int64_t>(flow_->getSize());
    }
  };

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  // OnTrigger method, implemented by NiFi CompressContent
  void onTrigger(core::ProcessContext* /*context*/, core::ProcessSession* /*session*/) override {
  }
  // OnTrigger method, implemented by NiFi CompressContent
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi CompressContent
  void initialize() override;

 private:
  static std::string toMimeType(CompressionFormat format);

  void processFlowFile(const std::shared_ptr<core::FlowFile>& flowFile, const std::shared_ptr<core::ProcessSession>& session);

  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  std::shared_ptr<logging::Logger> logger_;
  int compressLevel_{};
  CompressionMode compressMode_;
  ExtendedCompressionFormat compressFormat_;
  bool updateFileName_;
  bool encapsulateInTar_;
  uint32_t batchSize_{1};
  static const std::map<std::string, CompressionFormat> compressionFormatMimeTypeMap_;
  static const std::map<CompressionFormat, std::string> fileExtension_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

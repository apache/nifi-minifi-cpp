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
#ifndef __COMPRESS_CONTENT_H__
#define __COMPRESS_CONTENT_H__

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "archive_entry.h"
#include "archive.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define COMPRESSION_FORMAT_ATTRIBUTE "use mime.type attribute"
#define COMPRESSION_FORMAT_GZIP "gzip"
#define COMPRESSION_FORMAT_BZIP2 "bzip2"
#define COMPRESSION_FORMAT_XZ_LZMA2 "xz-lzma2"
#define COMPRESSION_FORMAT_LZMA "lzma"

#define MODE_COMPRESS "compress"
#define MODE_DECOMPRESS "decompress"

// CompressContent Class
class CompressContent: public core::Processor {
public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit CompressContent(std::string name, utils::Identifier uuid = utils::Identifier()) :
      core::Processor(name, uuid), logger_(logging::LoggerFactory<CompressContent>::getLogger()), updateFileName_(false) {
  }
  // Destructor
  virtual ~CompressContent() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "CompressContent";
  // Supported Properties
  static core::Property CompressMode;
  static core::Property CompressLevel;
  static core::Property CompressFormat;
  static core::Property UpdateFileName;

  // Supported Relationships
  static core::Relationship Failure;
  static core::Relationship Success;

public:
  // Nest Callback Class for read stream from flow for compress
  class ReadCallbackCompress: public InputStreamCallback {
  public:
    ReadCallbackCompress(std::shared_ptr<core::FlowFile> &flow, struct archive *arch, struct archive_entry *entry) :
        flow_(flow), arch_(arch), entry_(entry), status_(0), logger_(logging::LoggerFactory<CompressContent>::getLogger()) {
    }
    ~ReadCallbackCompress() {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int max_read = getpagesize();
      uint8_t buffer[max_read];
      int64_t ret = 0;
      uint64_t read_size = 0;

      ret = archive_write_header(arch_, entry_);
      if (ret != ARCHIVE_OK) {
        logger_->log_error("Compress Content archive error %s", archive_error_string(arch_));
        status_ = -1;
        return -1;
      }
      while (read_size < flow_->getSize()) {
        ret = stream->read(buffer, sizeof(buffer));
        if (ret < 0) {
          status_ = -1;
          return -1;
        }
        if (ret > 0) {
          ret = archive_write_data(arch_, buffer, ret);
          if (ret < 0) {
            logger_->log_error("Compress Content archive error %s", archive_error_string(arch_));
            status_ = -1;
            return -1;
          }
          read_size += ret;
        } else {
          break;
        }
      }
      return read_size;
    }
    std::shared_ptr<core::FlowFile> flow_;
    struct archive *arch_;
    struct archive_entry *entry_;
    int status_;
    std::shared_ptr<logging::Logger> logger_;
  };
  // Nest Callback Class for read stream from flow for decompress
  class ReadCallbackDecompress: public InputStreamCallback {
  public:
    ReadCallbackDecompress(std::shared_ptr<core::FlowFile> &flow) :
        read_size_(0), offset_(0), flow_(flow) {
      origin_offset_ = flow_->getOffset();
    }
    ~ReadCallbackDecompress() {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      read_size_ = 0;
      stream->seek(offset_);
      int readRet = stream->read(buffer_, sizeof(buffer_));
      read_size_ = readRet;
      if (readRet > 0) {
        offset_ += read_size_;
      }
      return readRet;
    }
    int64_t read_size_;
    uint8_t buffer_[8192];
    uint64_t offset_;
    uint64_t origin_offset_;
    std::shared_ptr<core::FlowFile> flow_;
  };
  // Nest Callback Class for write stream
  class WriteCallback: public OutputStreamCallback {
  public:
    WriteCallback(std::string &compress_mode, int64_t compress_level, std::string &compress_format,
        std::shared_ptr<core::FlowFile> &flow, const std::shared_ptr<core::ProcessSession> &session) :
        compress_mode_(compress_mode), compress_level_(compress_level), compress_format_(compress_format),
        flow_(flow), session_(session),
        logger_(logging::LoggerFactory<CompressContent>::getLogger()),
        readDecompressCb_(flow) {
      size_ = 0;
      stream_ = nullptr;
      status_ = 0;
    }
    ~WriteCallback() {
    }

    std::string compress_mode_;
    int64_t compress_level_;
    std::string compress_format_;
    std::shared_ptr<core::FlowFile> flow_;
    std::shared_ptr<core::ProcessSession> session_;
    std::shared_ptr<io::BaseStream> stream_;
    int64_t size_;
    std::shared_ptr<logging::Logger> logger_;
    CompressContent::ReadCallbackDecompress readDecompressCb_;
    int status_;

    static la_ssize_t archive_write(struct archive *arch, void *context, const void *buff, size_t size) {
      WriteCallback *callback = (WriteCallback *) context;
      la_ssize_t ret = callback->stream_->write(reinterpret_cast<uint8_t*>(const_cast<void*>(buff)), size);
      if (ret > 0)
        callback->size_ += (int64_t) ret;
      return ret;
    }

    static ssize_t archive_read(struct archive *arch, void *context, const void **buff) {
      WriteCallback *callback = (WriteCallback *) context;
      callback->session_->read(callback->flow_, &callback->readDecompressCb_);
      if (callback->readDecompressCb_.read_size_ >= 0) {
        *buff = callback->readDecompressCb_.buffer_;
        return callback->readDecompressCb_.read_size_;
      } else {
        archive_set_error(arch, EIO, "Error reading flowfile");
        return -1;
      }
    }

    static la_int64_t archive_skip(struct archive *a, void *client_data, la_int64_t request) {
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

    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      struct archive *arch;
      int r;

      if (compress_mode_ == MODE_COMPRESS) {
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
        if (compress_format_ == COMPRESSION_FORMAT_GZIP) {
          r = archive_write_add_filter_gzip(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
          std::string option;
          option = "gzip:compression-level=" + std::to_string((int) compress_level_);
          r = archive_write_set_options(arch, option.c_str());
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == COMPRESSION_FORMAT_BZIP2) {
          r = archive_write_add_filter_bzip2(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == COMPRESSION_FORMAT_LZMA) {
          r = archive_write_add_filter_lzma(arch);
          if (r != ARCHIVE_OK) {
            archive_write_log_error_cleanup(arch);
            return -1;
          }
        } else if (compress_format_ == COMPRESSION_FORMAT_XZ_LZMA2) {
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
        flow_->getAttribute(FlowAttributeKey(FILENAME), fileName);
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
        int entry_size = archive_entry_size(entry);
        logger_->log_debug("Decompress Content archive entry size %d", entry_size);
        size_ = 0;
        while (size_ < entry_size) {
          char buffer[8192];
          int ret = archive_read_data(arch, buffer, sizeof(buffer));
          if (ret < 0) {
            archive_read_log_error_cleanup(arch);
            return -1;
          }
          if (ret == 0)
            break;
          size_ += ret;
          ret = stream_->write(reinterpret_cast<uint8_t*>(buffer), ret);
          if (ret < 0) {
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

public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  // OnTrigger method, implemented by NiFi CompressContent
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session) {
  }
  // OnTrigger method, implemented by NiFi CompressContent
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
  // Initialize, over write by NiFi CompressContent
  virtual void initialize(void);

protected:

private:
  std::shared_ptr<logging::Logger> logger_;
  int64_t compressLevel_;
  std::string compressMode_;
  std::string compressFormat_;
  bool updateFileName_;
  std::map<std::string, std::string> compressionFormatMimeTypeMap_;
  std::map<std::string, std::string> fileExtension_;
};

REGISTER_RESOURCE (CompressContent, "Compresses or decompresses the contents of FlowFiles using a user-specified compression algorithm and updates the mime.type attribute as appropriate");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif

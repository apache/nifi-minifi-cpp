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
#pragma once

#include <curl/curl.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "Exception.h"
#include "core/logging/Logger.h"
#include "http/BaseHTTPClient.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::io {
class OutputStream;
class InputStream;
}
namespace org::apache::nifi::minifi::utils {

enum class SFTPError {
  Ok,
  PermissionDenied,
  FileDoesNotExist,
  FileAlreadyExists,
  CommunicationFailure,
  IoError,
  Unexpected
};

}  // namespace org::apache::nifi::minifi::utils

namespace magic_enum::customize {
using SFTPError = org::apache::nifi::minifi::utils::SFTPError;

template <>
constexpr customize_t enum_name<SFTPError>(SFTPError value) noexcept {
  switch (value) {
    case SFTPError::Ok:
      return "Ok";
    case SFTPError::PermissionDenied:
      return "Permission denied";
    case SFTPError::FileDoesNotExist:
      return "File does not exist";
    case SFTPError::FileAlreadyExists:
      return "File already exists";
    case SFTPError::CommunicationFailure:
      return "Communication failure";
    case SFTPError::IoError:
      return "IO error";
    case SFTPError::Unexpected:
      return "Unexpected";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::utils {

class SFTPException : public Exception {
 public:
  explicit SFTPException(const SFTPError err)
      :Exception{ExceptionType::FILE_OPERATION_EXCEPTION, fmt::format("SFTP Error: {0}", magic_enum::enum_name(err))},
      error_{err}
  {}
  [[nodiscard]] SFTPError error() const noexcept { return error_; }
 private:
  SFTPError error_;
};

class LastSFTPError {
 public:
  LastSFTPError();

  LastSFTPError(const LastSFTPError&) = delete;
  LastSFTPError(LastSFTPError&&) = delete;
  LastSFTPError& operator=(const LastSFTPError&) = delete;
  LastSFTPError& operator=(LastSFTPError&&) = delete;

  LastSFTPError& setLibssh2Error(unsigned long libssh2_sftp_error);  // NOLINT(runtime/int) unsigned long comes from libssh2 API
  LastSFTPError& setSftpError(const SFTPError& sftp_error);
  operator unsigned long() const;  // NOLINT(runtime/int) unsigned long comes from libssh2 API
  operator SFTPError() const;

 private:
  bool sftp_error_set_;
  unsigned long libssh2_sftp_error_;  // NOLINT(runtime/int) unsigned long comes from libssh2 API
  SFTPError sftp_error_;
};

class SFTPClient {
 public:
  SFTPClient(std::string hostname, uint16_t port, std::string username);

  ~SFTPClient();

  SFTPClient(const SFTPClient&) = delete;
  SFTPClient& operator=(const SFTPClient&) = delete;

  bool setVerbose();

  bool setHostKeyFile(const std::string& host_key_file_path, bool strict_host_checking);

  void setPasswordAuthenticationCredentials(const std::string& password);

  void setPublicKeyAuthenticationCredentials(const std::string& private_key_file_path, const std::string& private_key_passphrase);

  enum class ProxyType : uint8_t {
    Http,
    Socks
  };

  bool setProxy(ProxyType type, const http::HTTPProxy& proxy);

  bool setConnectionTimeout(std::chrono::milliseconds timeout);

  void setDataTimeout(std::chrono::milliseconds timeout);

  void setSendKeepAlive(bool send_keepalive);

  bool setUseCompression(bool use_compression);

  bool connect();

  bool sendKeepAliveIfNeeded(int &seconds_to_next);

  /**
   * If any function below this returns false, this function provides the last SFTP-related error.
   * If a function did not fail because of an SFTP-related error, this function will return SFTP_ERROR_OK.
   * If this function is called after a function returns true, the return value is UNDEFINED.
   */
  [[nodiscard]] SFTPError getLastError() const;

  std::optional<uint64_t> getFile(const std::string& path, io::OutputStream& output, int64_t expected_size = -1);

  std::optional<uint64_t> putFile(const std::string& path, io::InputStream& input, bool overwrite, int64_t expected_size = -1);

  bool rename(const std::string& source_path, const std::string& target_path, bool overwrite);

  bool createDirectoryHierarchy(const std::string& path);

  bool removeFile(const std::string& path);

  bool removeDirectory(const std::string& path);

  bool listDirectory(const std::string& path, bool follow_symlinks,
      std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>>& children_result);

  bool stat(const std::string& path, bool follow_symlinks, LIBSSH2_SFTP_ATTRIBUTES& result);

  static const uint32_t SFTP_ATTRIBUTE_PERMISSIONS = 0x00000001;
  static const uint32_t SFTP_ATTRIBUTE_UID         = 0x00000002;
  static const uint32_t SFTP_ATTRIBUTE_GID         = 0x00000004;
  static const uint32_t SFTP_ATTRIBUTE_MTIME       = 0x00000008;
  static const uint32_t SFTP_ATTRIBUTE_ATIME       = 0x00000010;
  struct SFTPAttributes {
    uint32_t flags;

    uint32_t permissions;
    uint64_t uid;
    uint64_t gid;
    int64_t mtime;
    int64_t atime;
  };

  bool setAttributes(const std::string& path, const SFTPAttributes& input);

 protected:
  /*
   * The maximum size libssh2 is willing to read or write in one go is 30000 bytes.
   * (See MAX_SFTP_OUTGOING_SIZE and MAX_SFTP_READ_SIZE).
   * So we will choose that as our read-write buffer size.
   */
  static constexpr size_t MAX_BUFFER_SIZE = 30000U;

  std::shared_ptr<core::logging::Logger> logger_;

  const std::string hostname_;
  const uint16_t port_;
  const std::string username_;

  LIBSSH2_KNOWNHOSTS *ssh_known_hosts_ = nullptr;
  bool strict_host_checking_ = false;

  bool password_authentication_enabled_ = false;
  std::string password_;

  bool public_key_authentication_enabled_ = false;
  std::string private_key_file_path_;
  std::string private_key_passphrase_;

  bool send_keepalive_ = false;

  std::vector<char> curl_errorbuffer_;

  CURL *easy_;
  LIBSSH2_SESSION *ssh_session_ = nullptr;
  LIBSSH2_SFTP *sftp_session_ = nullptr;

  bool connected_ = false;

  LastSFTPError last_error_;
};

}  // namespace org::apache::nifi::minifi::utils

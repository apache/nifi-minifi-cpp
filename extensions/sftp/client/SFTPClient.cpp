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
#include "SFTPClient.h"
#include <memory>
#include <set>
#include <vector>
#include <string>
#include <exception>
#include <sstream>
#include <iomanip>
#include "utils/StringUtils.h"
#include "utils/ScopeGuard.h"
#include "utils/StringUtils.h"
#include "utils/base64.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#define SFTP_ERROR(CODE) case CODE: \
                          return #CODE
static const char* sftp_strerror(unsigned long err) {
  switch (err) {
    SFTP_ERROR(LIBSSH2_FX_OK);
    SFTP_ERROR(LIBSSH2_FX_EOF);
    SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE);
    SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED);
    SFTP_ERROR(LIBSSH2_FX_FAILURE);
    SFTP_ERROR(LIBSSH2_FX_BAD_MESSAGE);
    SFTP_ERROR(LIBSSH2_FX_NO_CONNECTION);
    SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST);
    SFTP_ERROR(LIBSSH2_FX_OP_UNSUPPORTED);
    SFTP_ERROR(LIBSSH2_FX_INVALID_HANDLE);
    SFTP_ERROR(LIBSSH2_FX_NO_SUCH_PATH);
    SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS);
    SFTP_ERROR(LIBSSH2_FX_WRITE_PROTECT);
    SFTP_ERROR(LIBSSH2_FX_NO_MEDIA);
    SFTP_ERROR(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM);
    SFTP_ERROR(LIBSSH2_FX_QUOTA_EXCEEDED);
    SFTP_ERROR(LIBSSH2_FX_UNKNOWN_PRINCIPAL);
    SFTP_ERROR(LIBSSH2_FX_LOCK_CONFLICT);
    SFTP_ERROR(LIBSSH2_FX_DIR_NOT_EMPTY);
    SFTP_ERROR(LIBSSH2_FX_NOT_A_DIRECTORY);
    SFTP_ERROR(LIBSSH2_FX_INVALID_FILENAME);
    SFTP_ERROR(LIBSSH2_FX_LINK_LOOP);
    default:
      return "Unknown error";
  }
}

constexpr size_t SFTPClient::MAX_BUFFER_SIZE;

SFTPClient::SFTPClient(const std::string &hostname, uint16_t port, const std::string& username)
    : logger_(logging::LoggerFactory<SFTPClient>::getLogger()),
      hostname_(hostname),
      port_(port),
      username_(username),
      ssh_known_hosts_(nullptr),
      strict_host_checking_(false),
      password_authentication_enabled_(false),
      public_key_authentication_enabled_(false),
      data_timeout_(0),
      send_keepalive_(false),
      curl_errorbuffer_(CURL_ERROR_SIZE, '\0'),
      easy_(nullptr),
      ssh_session_(nullptr),
      sftp_session_(nullptr),
      connected_(false) {
  SFTPClientInitializer::getInstance()->initialize();
  easy_ = curl_easy_init();
  if (easy_ == nullptr) {
    throw std::runtime_error("Cannot create curl easy handle");
  }
  ssh_session_ = libssh2_session_init();
  if (ssh_session_ == nullptr) {
    curl_easy_cleanup(easy_);
    throw std::runtime_error("Cannot create ssh session handler");
  }
}

SFTPClient::~SFTPClient() {
    if (sftp_session_ != nullptr) {
      libssh2_sftp_shutdown(sftp_session_);
    }
    if (ssh_known_hosts_ != nullptr) {
      libssh2_knownhost_free(ssh_known_hosts_);
    }
    if (ssh_session_ != nullptr) {
      libssh2_session_disconnect(ssh_session_, "Normal Shutdown");
      libssh2_session_free(ssh_session_);
    }
    if (easy_ != nullptr) {
      curl_easy_cleanup(easy_);
    }
  logger_->log_trace("Closing SFTPClient for %s:%hu", hostname_, port_);
}

bool SFTPClient::setVerbose() {
  return curl_easy_setopt(easy_, CURLOPT_VERBOSE, 1L) == CURLE_OK;
}

bool SFTPClient::setHostKeyFile(const std::string& host_key_file_path, bool strict_host_checking) {
  if (ssh_known_hosts_ != nullptr) {
    return false;
  }
  ssh_known_hosts_ = libssh2_knownhost_init(ssh_session_);
  if (ssh_known_hosts_ == nullptr) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to init knownhost structure, error: %s", err_msg);
    return false;
  }
  if (libssh2_knownhost_readfile(ssh_known_hosts_, host_key_file_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH) <= 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to read host file %s, error: %s", host_key_file_path.c_str(), err_msg);
    return false;
  }
  strict_host_checking_ = strict_host_checking;
  return true;
}

void SFTPClient::setPasswordAuthenticationCredentials(const std::string& password) {
  password_authentication_enabled_ = true;
  password_ = password;
}

void SFTPClient::setPublicKeyAuthenticationCredentials(const std::string& private_key_file_path, const std::string& private_key_passphrase) {
  public_key_authentication_enabled_ = true;
  private_key_file_path_ = private_key_file_path;
  private_key_passphrase_ = private_key_passphrase;
}

bool SFTPClient::setProxy(ProxyType type, const utils::HTTPProxy& proxy) {
  switch (type) {
    case ProxyType::Http:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_HTTP) != CURLE_OK) {
        return false;
      }
      if (curl_easy_setopt(easy_, CURLOPT_HTTPPROXYTUNNEL, 1L) != CURLE_OK) {
        return false;
      }
      break;
    case ProxyType::Socks:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5) != CURLE_OK) {
        return false;
      }
      break;
  }
  std::stringstream proxy_string;
  proxy_string << proxy.host << ":" << proxy.port;
  if (curl_easy_setopt(easy_, CURLOPT_PROXY, proxy_string.str().c_str()) != CURLE_OK) {
    return false;
  }
  return true;
}

bool SFTPClient::setConnectionTimeout(int64_t timeout) {
  return curl_easy_setopt(easy_, CURLOPT_CONNECTTIMEOUT_MS, timeout) == CURLE_OK;
}

void SFTPClient::setDataTimeout(int64_t timeout) {
  data_timeout_ = timeout;
  libssh2_session_set_timeout(ssh_session_, timeout);
}

void SFTPClient::setSendKeepAlive(bool send_keepalive) {
  /*
   * Some SSH servers don't like if we send keepalives before we're connected,
   * but libssh2 sends keepalives before that, so we will set up keepalives after
   * a successful connection.
   */
  send_keepalive_ = send_keepalive;
}

bool SFTPClient::setUseCompression(bool use_compression) {
  return libssh2_session_flag(ssh_session_, LIBSSH2_FLAG_COMPRESS, 1) == 0;
}

bool SFTPClient::connect() {
  if (connected_) {
    return true;
  }

  /* Setting up curl request */
  std::stringstream uri_ss;
  uri_ss << hostname_ << ":" << port_;
  auto uri = uri_ss.str();
  if (curl_easy_setopt(easy_, CURLOPT_URL, uri.c_str()) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, curl_errorbuffer_.data()) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_CONNECT_ONLY, 1L) != CURLE_OK) {
    return false;
  }

  /* Connecting to proxy, if needed, then to the host */
  CURLcode curl_res = curl_easy_perform(easy_);
  if (curl_res != CURLE_OK) {
    logger_->log_error("Failed to connect to %s, curl error code: %s, detailed error message: %s",
        uri.c_str(),
        curl_easy_strerror(curl_res),
        curl_errorbuffer_.data());
    return false;
  }

  /* Getting socket from curl */
#ifdef WIN32
  curl_socket_t sockfd;
  /* Only CURLINFO_ACTIVESOCKET works on Win64 */
  curl_res = curl_easy_getinfo(easy_, CURLINFO_ACTIVESOCKET, &sockfd);
#else
  long sockfd;
  /* Some older cURL versions only support CURLINFO_LASTSOCKET */
  curl_res = curl_easy_getinfo(easy_, CURLINFO_LASTSOCKET, &sockfd);
#endif
  if (curl_res != CURLE_OK) {
    return false;
  }

  /* Establishing SSH connection */
  if (libssh2_session_handshake(ssh_session_, sockfd) != 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_info("Failed to establish SSH connection, error: %s", err_msg);
    return false;
  }

  /* Checking remote host */
  if (ssh_known_hosts_ != nullptr) {
    size_t hostkey_len = 0U;
    int type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    const char *hostkey = libssh2_session_hostkey(ssh_session_, &hostkey_len, &type);
    if (hostkey == nullptr) {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to get session hostkey, error: %s", err_msg);
      return false;
    }
    int keybit = 0;
    switch (type) {
      case LIBSSH2_HOSTKEY_TYPE_RSA:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
        break;
      case LIBSSH2_HOSTKEY_TYPE_DSS:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
        break;
      default:
        logger_->log_error("Unknown host key type: %d", type);
        return false;
    }
    struct libssh2_knownhost* known_host = nullptr;
    int keycheck_result = libssh2_knownhost_checkp(ssh_known_hosts_,
                            hostname_.c_str(),
                            -1 /*port*/,
                            hostkey, hostkey_len,
                            LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                            LIBSSH2_KNOWNHOST_KEYENC_RAW |
                            keybit,
                            &known_host);
    bool host_match = false;
    switch (keycheck_result) {
      case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
        logger_->log_warn("Failed to verify host key for %s", hostname_.c_str());
        break;
      case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
        logger_->log_warn("Host %s not found in the host key file", hostname_.c_str());
        break;
      case LIBSSH2_KNOWNHOST_CHECK_MISMATCH: {
        char* b64_out = nullptr;
        auto b64_len = Curl_base64_encode(hostkey, hostkey_len, &b64_out);
        logger_->log_warn("Host key mismatch for %s, expected: %s, actual: %s", hostname_.c_str(),
                          known_host == nullptr ? "" : known_host->key,
                          b64_out == nullptr ? "" : std::string(b64_out, b64_len).c_str());
        break;
      }
      case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        host_match = true;
        logger_->log_debug("Host key verification succeeded for %s", hostname_.c_str());
        break;
      default:
        logger_->log_error("Unknown libssh2_knownhost_checkp result: %d", keycheck_result);
        break;
    }
    if (strict_host_checking_ && !host_match) {
      return false;
    }
  } else {
    const char* fingerprint = libssh2_hostkey_hash(ssh_session_, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (fingerprint == nullptr) {
      logger_->log_warn("Cannot get remote server fingerprint");
    } else {
      std::stringstream fingerprint_hex;
      for (size_t i = 0; i < 20; i++) {
        fingerprint_hex << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(static_cast<uint8_t>(fingerprint[i]));
        if (i != 19) {
          fingerprint_hex << ":";
        }
      }
      logger_->log_debug("SHA1 host key fingerprint for %s is %s", hostname_.c_str(), fingerprint_hex.str().c_str());
    }
  }

  /* Getting possible authentication methods */
  bool authenticated = false;
  std::set<std::string> auth_methods;
  char* userauthlist = libssh2_userauth_list(ssh_session_, username_.c_str(), strlen(username_.c_str()));
  if (userauthlist == nullptr) {
    if (libssh2_userauth_authenticated(ssh_session_) == 1) {
      authenticated = true;
      logger_->log_warn("SSH server authenticated with SSH_USERAUTH_NONE - this is unusual");
    } else {
      logger_->log_error("Failed to get supported SSH authentication methods");
      return false;
    }
  } else {
    auto methods_split = utils::StringUtils::split(userauthlist, ",");
    auth_methods.insert(std::make_move_iterator(methods_split.begin()), std::make_move_iterator(methods_split.end()));
  }

  /* Authenticating */
  if (!authenticated && public_key_authentication_enabled_ && auth_methods.count("publickey") == 1) {
    if (libssh2_userauth_publickey_fromfile_ex(ssh_session_,
                                               username_.c_str(),
                                               username_.length(),
                                               nullptr /*publickey*/,
                                               private_key_file_path_.c_str(),
                                               private_key_passphrase_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with publickey");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with publickey, error: %s", err_msg);
    }
  }
  if (!authenticated && password_authentication_enabled_ && auth_methods.count("password") == 1) {
    if (libssh2_userauth_password(ssh_session_, username_.c_str(), password_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with password");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with password, error: %s", err_msg);
    }
  }
  if (!authenticated) {
    logger_->log_error("Could not authenticate with any available method");
    return false;
  }

  /* Initializing SFTP session */
  sftp_session_ = libssh2_sftp_init(ssh_session_);
  if (sftp_session_ == nullptr) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to initialize SFTP session, error: %s", err_msg);
    return false;
  }

  connected_ = true;

  /* Set up keepalive config if needed */
  if (send_keepalive_) {
    libssh2_keepalive_config(ssh_session_, 0 /*want_reply*/, 10U /*interval*/);
  }

  return true;
}

bool SFTPClient::sendKeepAliveIfNeeded(int &seconds_to_next) {
  if (libssh2_keepalive_send(ssh_session_, &seconds_to_next) != 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to send keepalive to %s@%s:%hu, error: %s",
        username_,
        hostname_,
        port_,
        err_msg);
    return false;
  }
  return true;
}

bool SFTPClient::getFile(const std::string& path, io::BaseStream& output, int64_t expected_size /*= -1*/) {
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open(sftp_session_, path.c_str(), LIBSSH2_FXF_READ, 0);
  if (file_handle == nullptr) {
    logger_->log_error("Failed to open remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  utils::ScopeGuard guard([&file_handle]() {
    libssh2_sftp_close(file_handle);
  });

  const size_t buf_size = expected_size < 0 ? MAX_BUFFER_SIZE : std::min<size_t>(expected_size, MAX_BUFFER_SIZE);
  std::vector<uint8_t> buf(buf_size);
  uint64_t total_read = 0U;
  do {
    ssize_t read_ret = libssh2_sftp_read(file_handle, reinterpret_cast<char*>(buf.data()), buf.size());
    if (read_ret < 0) {
      logger_->log_error("Failed to read remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
      return false;
    } else if (read_ret == 0) {
      logger_->log_trace("EOF while reading remote file \"%s\"", path.c_str());
      break;
    }
    logger_->log_trace("Read %d bytes from remote file \"%s\"", read_ret, path.c_str());
    total_read += read_ret;
    int remaining = read_ret;
    while (remaining > 0) {
      int write_ret = output.writeData(buf.data() + (buf.size() - remaining), remaining);
      if (write_ret < 0) {
        logger_->log_error("Failed to write output");
        return false;
      }
      remaining -= write_ret;
    }
  } while (true);

  if (expected_size >= 0 && total_read != expected_size) {
    logger_->log_error("Remote file \"%s\" has unexpected size, expected: %ld, actual: %lu", path.c_str(), expected_size, total_read);
    return false;
  }

  return true;
}

bool SFTPClient::putFile(const std::string& path, io::BaseStream& input, bool overwrite, int64_t expected_size /*= -1*/) {
  int flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | (overwrite ? LIBSSH2_FXF_TRUNC : LIBSSH2_FXF_EXCL);
  logger_->log_trace("Opening remote file \"%s\"", path.c_str());
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open(sftp_session_, path.c_str(), flags, 0644);
  if (file_handle == nullptr) {
    logger_->log_error("Failed to open remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  utils::ScopeGuard guard([this, &file_handle, &path]() {
    logger_->log_trace("Closing remote file \"%s\"", path.c_str());
    libssh2_sftp_close(file_handle);
  });

  /* If they just want a zero byte file, we are done */
  if (expected_size == 0) {
    return true;
  }

  const size_t buf_size = expected_size < 0 ? MAX_BUFFER_SIZE : std::min<size_t>(expected_size, MAX_BUFFER_SIZE);
  std::vector<uint8_t> buf(buf_size);
  uint64_t total_read = 0U;
  do {
    int read_ret = input.readData(buf.data(), buf.size());
    if (read_ret < 0) {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_error("Error while reading input");
      return false;
    } else if (read_ret == 0) {
      logger_->log_trace("EOF while reading input");
      break;
    }
    logger_->log_trace("Read %d bytes", read_ret);
    total_read += read_ret;
    ssize_t remaining = read_ret;
    while (remaining > 0) {
      int write_ret = libssh2_sftp_write(file_handle, reinterpret_cast<char*>(buf.data() + (read_ret - remaining)), remaining);
      if (write_ret < 0) {
        logger_->log_error("Failed to write remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
        return false;
      }
      logger_->log_trace("Wrote %d bytes to remote file \"%s\"", write_ret, path.c_str());
      remaining -= write_ret;
    }
  } while (true);

  if (expected_size >= 0 && total_read != expected_size) {
    logger_->log_error("Input has unexpected size, expected: %ld, actual: %lu", path.c_str(), expected_size, total_read);
    return false;
  }

  return true;
}

bool SFTPClient::rename(const std::string& source_path, const std::string& target_path, bool overwrite) {
  int flags = 0;
  if (overwrite) {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE | LIBSSH2_SFTP_RENAME_OVERWRITE;
  } else {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE;
  }
  bool tried_deleting = false;
  while (libssh2_sftp_rename_ex(sftp_session_,
                              source_path.c_str(),
                              source_path.length(),
                              target_path.c_str(),
                              target_path.length(),
                              flags) != 0) {
    auto err = libssh2_sftp_last_error(sftp_session_);
    if (overwrite && err == LIBSSH2_FX_FILE_ALREADY_EXISTS && !tried_deleting) {
      /* It couldn't overwrite the file, let's delete it and try again */
      logger_->log_debug("Failed to overwrite file \"%s\" with rename, deleting instead", target_path.c_str());
      tried_deleting = true;
      if (!this->removeFile(target_path)) {
        return false;
      }
      continue;
    }
    logger_->log_error("Failed to rename remote file \"%s\" to \"%s\", error: %s",
        source_path.c_str(),
        target_path.c_str(),
        sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

bool SFTPClient::createDirectoryHierarchy(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  bool absolute = path[0] == '/';
  auto elements = utils::StringUtils::split(path, "/");
  std::stringstream dir;
  if (absolute) {
    dir << "/";
  }
  for (const auto& element : elements) {
    dir << element << "/";
    auto current_dir = dir.str();
    int res = libssh2_sftp_mkdir_ex(sftp_session_, current_dir.c_str(), current_dir.length(), 0755);
    if (res < 0) {
      auto err = libssh2_sftp_last_error(sftp_session_);
      if (err != LIBSSH2_FX_FILE_ALREADY_EXISTS &&
          err != LIBSSH2_FX_FAILURE &&
          err != LIBSSH2_FX_PERMISSION_DENIED) {
        logger_->log_error("Failed to create remote directory \"%s\", error: %s", current_dir.c_str(), sftp_strerror(err));
        return false;
      } else {
        logger_->log_debug("Non-fatal failure to create remote directory \"%s\", error: %s", current_dir.c_str(), sftp_strerror(err));
      }
    }
  }
  return true;
}

bool SFTPClient::removeFile(const std::string& path) {
  if (libssh2_sftp_unlink(sftp_session_, path.c_str()) != 0) {
    logger_->log_error("Failed to remove remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

bool SFTPClient::removeDirectory(const std::string& path) {
  if (libssh2_sftp_rmdir(sftp_session_, path.c_str()) != 0) {
    logger_->log_error("Failed to remove remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

bool SFTPClient::listDirectory(const std::string& path, bool follow_symlinks,
    std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>>& children_result) {
  LIBSSH2_SFTP_HANDLE *dir_handle = libssh2_sftp_open_ex(sftp_session_,
                                                          path.c_str(),
                                                          path.length(),
                                                          0 /* flags */,
                                                          0 /* mode */,
                                                          LIBSSH2_SFTP_OPENDIR);
  if (dir_handle == nullptr) {
    logger_->log_error("Failed to open remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  utils::ScopeGuard guard([&dir_handle]() {
    libssh2_sftp_close(dir_handle);
  });

  LIBSSH2_SFTP_ATTRIBUTES attrs;
  std::vector<char> filename(4096U);
  std::vector<char> longentry(4096U);
  do {
    int ret = libssh2_sftp_readdir_ex(dir_handle,
                                      filename.data(),
                                      filename.size(),
                                      longentry.data(),
                                      longentry.size(),
                                      &attrs);
    if (ret < 0) {
      logger_->log_error("Failed to read remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
      return false;
    } else if (ret == 0) {
      break;
    }
    if (follow_symlinks && attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && LIBSSH2_SFTP_S_ISLNK(attrs.permissions)) {
      auto orig_attrs = attrs;
      bool file_not_exists;
      if (!this->stat(path, true /*follow_symlinks*/, attrs, file_not_exists)) {
        logger_->log_debug("Failed to stat directory child \"%s/%s\", error: %s", path.c_str(), filename.data(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
        attrs = orig_attrs;
      }
    }
    children_result.emplace_back(std::string(filename.data()), std::string(longentry.data()), std::move(attrs));
  } while (true);
  return true;
}

bool SFTPClient::stat(const std::string& path, bool follow_symlinks, LIBSSH2_SFTP_ATTRIBUTES& result, bool& file_not_exists) {
  file_not_exists = false;
  if (libssh2_sftp_stat_ex(sftp_session_,
                            path.c_str(),
                            path.length(),
                            follow_symlinks ? LIBSSH2_SFTP_STAT : LIBSSH2_SFTP_LSTAT,
                            &result) != 0) {
    auto error = libssh2_sftp_last_error(sftp_session_);
    if (error == LIBSSH2_FX_NO_SUCH_FILE) {
      file_not_exists = true;
    }
    logger_->log_debug("Failed to stat remote path \"%s\", error: %s", path.c_str(), sftp_strerror(error));
    return false;
  }
  return true;
}

bool SFTPClient::setAttributes(const std::string& path, const SFTPAttributes& input) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  memset(&attrs, 0x00, sizeof(attrs));
  if ((!!(input.flags & SFTP_ATTRIBUTE_UID) != !!(input.flags & SFTP_ATTRIBUTE_GID)) ||
      (!!(input.flags & SFTP_ATTRIBUTE_MTIME) != !!(input.flags & SFTP_ATTRIBUTE_ATIME))) {
    /* Because we can only set these attributes in pairs, we must stat first to learn the other */
    bool file_not_exists;
    if (!this->stat(path, false /*follow_symlinks*/, attrs, file_not_exists)) {
      return false;
    }
  }
  attrs.flags = 0U;
  if (input.flags & SFTP_ATTRIBUTE_PERMISSIONS) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = input.permissions;
  }
  if (input.flags & SFTP_ATTRIBUTE_UID) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_UIDGID;
    attrs.uid = input.uid;
  }
  if (input.flags & SFTP_ATTRIBUTE_GID) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_UIDGID;
    attrs.gid = input.gid;
  }
  if (input.flags & SFTP_ATTRIBUTE_MTIME) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_ACMODTIME;
    attrs.mtime = input.mtime;
  }
  if (input.flags & SFTP_ATTRIBUTE_ATIME) {
    attrs.flags |= LIBSSH2_SFTP_ATTR_ACMODTIME;
    attrs.atime = input.atime;
  }

  if (libssh2_sftp_stat_ex(sftp_session_,
                           path.c_str(),
                           path.length(),
                           LIBSSH2_SFTP_SETSTAT,
                           &attrs) != 0) {
    logger_->log_debug("Failed to setstat on remote path \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }

  return true;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

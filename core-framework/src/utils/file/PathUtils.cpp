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

#include "utils/file/PathUtils.h"

#ifdef WIN32
#include <Windows.h>
#include "utils/UnicodeConversion.h"
#else
#include <sys/statvfs.h>
#include <climits>
#include <cstdlib>
#endif

#include <iostream>
#include "utils/file/FileUtils.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::utils::file {

std::string globToRegex(std::string glob) {
  utils::string::replaceAll(glob, ".", "\\.");
  utils::string::replaceAll(glob, "*", ".*");
  utils::string::replaceAll(glob, "?", ".");
  return glob;
}

space_info space(const path path, std::error_code& ec) noexcept {
  constexpr auto kErrVal = gsl::narrow_cast<std::uintmax_t>(-1);
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
  struct statvfs svfs{};
  const int statvfs_retval = statvfs(path, &svfs);
  if (statvfs_retval == -1) {
    const std::error_code err_code{errno, std::generic_category()};
    ec = err_code;
    return space_info{kErrVal, kErrVal, kErrVal};
  }
  const auto capacity = std::uintmax_t{svfs.f_blocks} * svfs.f_frsize;
  const auto free = std::uintmax_t{svfs.f_bfree} * svfs.f_frsize;
  const auto available = std::uintmax_t{svfs.f_bavail} * svfs.f_frsize;
#elif defined(_WIN32)
  ULARGE_INTEGER free_bytes_available_to_caller;
  ULARGE_INTEGER total_number_of_bytes;
  ULARGE_INTEGER total_number_of_free_bytes;
  const bool get_disk_free_space_ex_success = GetDiskFreeSpaceEx(path, &free_bytes_available_to_caller, &total_number_of_bytes,
      &total_number_of_free_bytes);
  if (!get_disk_free_space_ex_success) {
    const std::error_code err_code{gsl::narrow<int>(GetLastError()), std::system_category()};
    ec = err_code;
    return space_info{kErrVal, kErrVal, kErrVal};
  }
  const auto capacity = total_number_of_bytes.QuadPart;
  const auto free = total_number_of_free_bytes.QuadPart;
  const auto available = free_bytes_available_to_caller.QuadPart;
#else
  const auto capacity = kErrVal;
  const auto free = kErrVal;
  const auto available = kErrVal;
#endif /* unix || apple */

  return space_info{capacity, free, available};
}

space_info space(const path path) {
  std::error_code ec;
  const auto result = space(path, ec);  // const here doesn't break NRVO
  if (ec) {
    throw filesystem_error{ec.message(), path, "", ec};
  }
  return result;
}

namespace {

// RFC 3986 unreserved set. Spelled out rather than using std::isalnum, which is locale-dependent
// and reports bytes 0x80-0xFF as alphanumeric under some locales, leaking raw non-ASCII bytes.
constexpr bool isUnreservedUriChar(const char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
      || ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

constexpr bool isAsciiAlpha(const char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

void appendEncoded(std::string& uri, const std::string_view bytes, const std::string_view literal_chars) {
  for (const char ch : bytes) {
    if (isUnreservedUriChar(ch) || literal_chars.contains(ch)) {
      uri += ch;
    } else {
      uri += '%';
      uri += string::to_hex(std::string_view{&ch, 1}, true);
    }
  }
}

// ':' is a valid pchar (RFC 3986 3.3) and the path we emit is always absolute, so a colon can never
// be mistaken for the scheme separator. Keeping it literal matches Node and Java, and keeps Windows
// drive letters readable in the transit URI.
void appendPath(std::string& uri, const std::string_view path) {
  appendEncoded(uri, path, "/:");
}

// reg-name = *( unreserved / pct-encoded / sub-delims ), so percent-escapes are legal in a host.
// Host names are case-insensitive, so lowercase them: one share should yield one transit URI.
void appendHost(std::string& uri, const std::string_view host) {
  std::string lowercase_host{host};
  for (char& ch : lowercase_host) {
    if (ch >= 'A' && ch <= 'Z') { ch += 'a' - 'A'; }
  }
  appendEncoded(uri, lowercase_host, "!$&'()*+,;=");
}

// host_and_path is a UNC path with its leading "//" already removed, e.g. "server/share/file".
std::expected<std::string, std::string> uncPathToUri(const std::string_view host_and_path) {
  const auto host_end = host_and_path.find('/');
  const auto host = host_and_path.substr(0, host_end);
  if (host.empty()) {
    return std::unexpected{"UNC path '//" + std::string(host_and_path) + "' has no host"};
  }
  std::string uri = "file://";
  appendHost(uri, host);
  if (host_end != std::string_view::npos) {
    appendPath(uri, host_and_path.substr(host_end));
  }
  return uri;
}

std::expected<std::string, std::string> windowsPathToUri(std::string_view path) {
  // Extended-length (\\?\) and device (\\.\) prefixes are not part of the path proper, and the host
  // position of a file URI has no meaning for them. Strip the prefix and reinterpret what is left.
  if (path.starts_with("//?/") || path.starts_with("//./")) {
    path.remove_prefix(4);
    if (path.starts_with("UNC/")) {
      path.remove_prefix(4);
      return uncPathToUri(path);
    }
    if (!(path.size() >= 2 && isAsciiAlpha(path[0]) && path[1] == ':')) {
      return std::unexpected{"Device-namespace path '" + std::string(path) + "' cannot be expressed as a file URI"};
    }
  } else if (path.starts_with("//")) {
    return uncPathToUri(path.substr(2));
  }

  // A drive letter must be followed by a separator or nothing at all. "C:foo" is relative to the
  // current directory *of drive C* rather than to its root, so it is not expressible on its own.
  const bool has_drive = path.size() >= 2 && isAsciiAlpha(path[0]) && path[1] == ':'
      && (path.size() == 2 || path[2] == '/');
  if (!has_drive) {
    return std::unexpected{"Windows path '" + std::string(path) + "' is not fully qualified"};
  }

  std::string uri = "file:///";
  uri += gsl::narrow_cast<char>(std::toupper(static_cast<unsigned char>(path[0])));
  appendPath(uri, path.substr(1));
  return uri;
}

std::expected<std::string, std::string> posixPathToUri(const std::string_view path) {
  if (!path.starts_with('/')) {
    return std::unexpected{"POSIX path '" + std::string(path) + "' is not absolute"};
  }
  // Always emit the empty authority, so the result has at least three slashes. This is also what
  // makes a path that itself begins with "//" come out right: "//x" must become "file:////x", since
  // "file://x" would name a host instead.
  std::string uri = "file://";
  appendPath(uri, path);
  return uri;
}

}  // namespace

std::expected<std::string, std::string> genericPathToUri(const std::string_view generic_path, const PathFlavor flavor) {
  if (generic_path.empty()) {
    return std::unexpected{"Cannot convert an empty path to a file URI"};
  }
  switch (flavor) {
    case PathFlavor::Windows: return windowsPathToUri(generic_path);
    case PathFlavor::Posix: return posixPathToUri(generic_path);
  }
  return std::unexpected{"Unknown path flavor"};
}

std::expected<std::string, std::string> pathToUri(const std::filesystem::path& path) {
  if (path.empty()) {
    return std::unexpected{"Cannot convert an empty path to a file URI"};
  }

  std::error_code absolute_error;
  const auto absolute_path = std::filesystem::absolute(path, absolute_error);
  if (absolute_error) {
    return std::unexpected{"Could not resolve '" + path.string() + "' to an absolute path: " + absolute_error.message()};
  }

#ifdef WIN32
  // Not generic_string(): that converts UTF-16 through the ANSI code page and replaces anything
  // unrepresentable there with '?', which would silently point the URI at a different file.
  const std::string generic_path = utils::to_string(absolute_path.generic_wstring());
#else
  // POSIX file names are byte strings and need not be valid UTF-8, so percent-encode them as given.
  const std::string generic_path = absolute_path.generic_string();
#endif

  return genericPathToUri(generic_path, native_path_flavor);
}

}  // namespace org::apache::nifi::minifi::utils::file

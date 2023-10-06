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

#include "utils/TestUtils.h"

#include <type_traits>

#ifdef WIN32
#include <windows.h>
#include <aclapi.h>
#endif

#include "utils/gsl.h"

#ifdef WIN32
namespace {

void setAclOnFileOrDirectory(std::string file_name, DWORD perms, ACCESS_MODE perm_options) {
  PSECURITY_DESCRIPTOR security_descriptor = nullptr;
  const auto security_descriptor_deleter = gsl::finally([&security_descriptor] { if (security_descriptor) { LocalFree((HLOCAL) security_descriptor); } });

  PACL old_acl = nullptr;  // GetNamedSecurityInfo will set this to a non-owning pointer to a field inside security_descriptor: no need to free it
  if (GetNamedSecurityInfo(file_name.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &old_acl, NULL, &security_descriptor) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not get security info for file: " + file_name);
  }

  char trustee_name[] = "Everyone";
  EXPLICIT_ACCESS explicit_access = {
    .grfAccessPermissions = perms,
    .grfAccessMode = perm_options,
    .grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
    .Trustee = { .TrusteeForm = TRUSTEE_IS_NAME, .ptstrName = trustee_name }
  };

  PACL new_acl = nullptr;
  const auto new_acl_deleter = gsl::finally([&new_acl] { if (new_acl) { LocalFree((HLOCAL) new_acl); } });

  if (SetEntriesInAcl(1, &explicit_access, old_acl, &new_acl) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not create new ACL for file: " + file_name);
  }

  if (SetNamedSecurityInfo(file_name.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, new_acl, NULL) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not set the new ACL for file: " + file_name);
  }
}

}  // namespace
#endif

namespace org::apache::nifi::minifi::utils {

#ifdef WIN32
// If minifi is not installed through the MSI installer, then TZDATA might be missing
// date::set_install can point to the TZDATA location, but it has to be called from each library/executable that wants to use timezones
void dateSetInstall(const std::string& install) {
  date::set_install(install);
}
#endif

void makeFileOrDirectoryNotWritable(const std::filesystem::path& file_name) {
#ifdef WIN32
  setAclOnFileOrDirectory(file_name.string(), FILE_GENERIC_WRITE, DENY_ACCESS);
#else
  std::filesystem::permissions(file_name, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
#endif
}

void makeFileOrDirectoryWritable(const std::filesystem::path& file_name) {
#ifdef WIN32
  setAclOnFileOrDirectory(file_name.string(), FILE_GENERIC_WRITE, GRANT_ACCESS);
#else
  std::filesystem::permissions(file_name, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
#endif
}

}  // namespace org::apache::nifi::minifi::utils

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};
use std::path::Path;

#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

#[cfg(unix)]
pub struct UnixPermission {}

#[cfg(unix)]
impl PropertySchema for UnixPermission {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = true;
}

#[cfg(unix)]
impl PropertyType for UnixPermission {
    type Output = std::fs::Permissions;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        Ok(std::fs::Permissions::from_mode(u32::from_str_radix(s, 8)?))
    }
}

#[cfg(unix)]
#[derive(Debug)]
pub(super) struct PutFileUnixPermissions {
    pub(crate) file_permissions: Option<std::fs::Permissions>,
    pub(crate) directory_permissions: Option<std::fs::Permissions>,
}

#[cfg(unix)]
impl PutFileUnixPermissions {
    pub(crate) fn set_directory_permissions(&self, path: &Path) -> std::io::Result<()> {
        if let Some(permissions) = self.directory_permissions.clone() {
            return std::fs::set_permissions(path, permissions);
        }
        Ok(())
    }

    pub(crate) fn set_file_permissions(&self, file: &Path) -> std::io::Result<()> {
        if let Some(permissions) = self.file_permissions.clone() {
            return std::fs::set_permissions(file, permissions);
        }
        Ok(())
    }
}

#[cfg(windows)]
#[derive(Debug)]
pub(crate) struct PutFileUnixPermissions {}

#[cfg(windows)]
impl PutFileUnixPermissions {
    pub(crate) fn set_directory_permissions(&self, _path: &Path) -> std::io::Result<()> {
        Ok(())
    }

    pub(crate) fn set_file_permissions(&self, _file: &Path) -> std::io::Result<()> {
        Ok(())
    }
}

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

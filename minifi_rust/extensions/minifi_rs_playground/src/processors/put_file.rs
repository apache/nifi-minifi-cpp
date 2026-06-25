use crate::processors::put_file::relationships::{FAILURE, SUCCESS};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetProperty, InputStream, Logger,
    MinifiError, Schedule, TransformedFlowFile, trace, warn,
};
use std::path::{Path, PathBuf};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};
use walkdir::WalkDir;

mod properties;
mod relationships;
#[cfg(unix)]
mod unix_only_properties;

#[derive(Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr)]
#[strum(serialize_all = "camelCase", const_into_str)]
enum ConflictResolutionStrategy {
    Fail,
    Replace,
    Ignore,
}

#[cfg(unix)]
#[derive(Debug)]
struct PutFileUnixPermissions {
    file_permissions: Option<std::fs::Permissions>,
    directory_permissions: Option<std::fs::Permissions>,
}

#[cfg(unix)]
impl PutFileUnixPermissions {
    fn set_directory_permissions(&self, path: &Path) -> std::io::Result<()> {
        if let Some(permissions) = self.directory_permissions.as_ref().map(|p| p.clone()) {
            return std::fs::set_permissions(path, permissions);
        }
        Ok(())
    }

    fn set_file_permissions(&self, file: &Path) -> std::io::Result<()> {
        if let Some(permissions) = self.file_permissions.as_ref().map(|p| p.clone()) {
            return std::fs::set_permissions(file, permissions);
        }
        Ok(())
    }
}

#[cfg(windows)]
#[derive(Debug)]
struct PutFileUnixPermissions {}

#[cfg(windows)]
impl PutFileUnixPermissions {
    fn set_directory_permissions(&self, _path: &Path) -> std::io::Result<()> {
        Ok(())
    }

    fn set_file_permissions(&self, _file: &Path) -> std::io::Result<()> {
        Ok(())
    }
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct PutFileRs {
    conflict_resolution_strategy: ConflictResolutionStrategy,
    try_make_dirs: bool,
    maximum_file_count: Option<u64>,
    unix_permissions: PutFileUnixPermissions,
}

impl PutFileRs {
    pub(crate) fn directory_is_full(&self, p0: &Path) -> bool {
        if let Some(max_file_count) = self.maximum_file_count
            && let Some(parent) = p0.parent()
        {
            parent.exists()
                && WalkDir::new(parent)
                    .into_iter()
                    .filter_map(Result::ok)
                    .filter(|e| e.file_type().is_file())
                    .count()
                    >= max_file_count as usize
        } else {
            false
        }
    }

    fn get_destination_path<Ctx>(context: &Ctx) -> Result<PathBuf, MinifiError>
    where
        Ctx: GetProperty + GetAttribute,
    {
        let directory = context
            .get_property(&properties::DIRECTORY)?
            .expect("required property");

        let file_name = context
            .get_attribute("filename")?
            .unwrap_or("foo.txt".to_string()); // TODO fallback to UUID
        Ok(PathBuf::from(directory + "/" + file_name.as_str()))
    }

    fn prepare_destination(&self, destination: &Path) -> std::io::Result<()> {
        if let Some(parent) = destination.parent() {
            if self.try_make_dirs {
                std::fs::create_dir_all(parent)?;
                self.unix_permissions.set_directory_permissions(parent)?;
            }
        }
        Ok(())
    }

    fn put_file<L>(
        &self,
        input_stream: &mut dyn InputStream,
        logger: &L,
        destination: &Path,
    ) -> Result<(), MinifiError>
    where
        L: Logger,
    {
        match self.prepare_destination(destination) {
            Ok(_) => {}
            Err(err) => {
                warn!(logger, "Failed to prepare destination due to {:?}", err);
            }
        }
        let mut file = std::fs::File::create(destination)?;
        std::io::copy(input_stream, &mut file)?;
        match self.unix_permissions.set_file_permissions(destination) {
            Ok(_) => {}
            Err(err) => {
                warn!(logger, "Failed to set file permissions due to {:?}", err);
            }
        }
        Ok(())
    }

    #[cfg(unix)]
    fn parse_unix_permissions<P: GetProperty>(
        context: &P,
    ) -> Result<PutFileUnixPermissions, MinifiError> {
        use std::os::unix::fs::PermissionsExt;
        let parse_permission =
            |property: &minifi_native::Property| -> Result<Option<std::fs::Permissions>, MinifiError> {
                Ok(context
                    .get_property(&property)?
                    .map(|perm_str| u32::from_str_radix(&perm_str, 8))
                    .transpose()?
                    .map(|perm| std::fs::Permissions::from_mode(perm)))
            };
        let file_permissions = parse_permission(&unix_only_properties::PERMISSIONS)?;
        let directory_permissions = parse_permission(&unix_only_properties::DIRECTORY_PERMISSIONS)?;

        Ok(PutFileUnixPermissions {
            file_permissions,
            directory_permissions,
        })
    }

    #[cfg(windows)]
    fn parse_unix_permissions<P: GetProperty>(
        _context: &P,
    ) -> Result<PutFileUnixPermissions, MinifiError> {
        Ok(PutFileUnixPermissions {})
    }
}

impl Schedule for PutFileRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError> {
        let conflict_resolution_strategy = context
            .get_property(&properties::CONFLICT_RESOLUTION)?
            .expect("required property")
            .parse::<ConflictResolutionStrategy>()?;

        let try_make_dirs = context
            .get_bool_property(&properties::CREATE_DIRS)?
            .expect("required property");

        let maximum_file_count = context.get_u64_property(&properties::MAX_FILE_COUNT)?;

        let unix_permissions = Self::parse_unix_permissions(context)?;

        Ok(PutFileRs {
            conflict_resolution_strategy,
            try_make_dirs,
            maximum_file_count,
            unix_permissions,
        })
    }
}

impl FlowFileTransform for PutFileRs {
    fn transform<
        'a,
        Context: GetProperty + GetControllerService + GetAttribute,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, MinifiError> {
        trace!(logger, "on_trigger: {:?}", self);

        let Ok(destination_path) = Self::get_destination_path(context) else {
            warn!(logger, "Invalid destination path");
            return Ok(TransformedFlowFile::route_without_changes(&FAILURE));
        };

        if self.directory_is_full(&destination_path) {
            warn!(logger, "Directory is full");
            return Ok(TransformedFlowFile::route_without_changes(&FAILURE));
        }

        if destination_path.exists() {
            match self.conflict_resolution_strategy {
                ConflictResolutionStrategy::Fail => {
                    return Ok(TransformedFlowFile::route_without_changes(&FAILURE));
                }
                ConflictResolutionStrategy::Replace => {
                    // continue with PutFile operation
                }
                ConflictResolutionStrategy::Ignore => {
                    return Ok(TransformedFlowFile::route_without_changes(&SUCCESS));
                }
            }
        }

        match self.put_file(input_stream, logger, &destination_path) {
            Ok(_) => Ok(TransformedFlowFile::route_without_changes(&SUCCESS)),
            Err(_e) => Ok(TransformedFlowFile::route_without_changes(&FAILURE)),
        }
    }
}

pub(crate) mod processor_definition;

#[cfg(test)]
mod tests;

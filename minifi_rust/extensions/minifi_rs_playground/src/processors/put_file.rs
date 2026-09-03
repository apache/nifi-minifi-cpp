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

// This is the (not production ready) reimplementation of the already existing standard PutFile processor

use crate::processors::put_file::relationships::{FAILURE, SUCCESS};
use crate::processors::put_file::unix_permissions::PutFileUnixPermissions;
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    FlowFileTransform, GetAttribute, GetControllerService, GetId, GetProperty, InputStream, Logger,
    MinifiError, ProcessError, RouteErrorExt, Schedule, TransformedFlowFile, trace, warn,
};
use std::path::{Path, PathBuf};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};
use walkdir::WalkDir;

mod properties;
mod relationships;
#[cfg(unix)]
mod unix_only_properties;

mod unix_permissions;

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "camelCase", const_into_str)]
enum ConflictResolutionStrategy {
    Fail,
    Replace,
    Ignore,
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
                    .max_depth(1)
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
        Ctx: GetProperty + GetAttribute + GetId,
    {
        let directory = context.get_property(&properties::DIRECTORY)?;

        let file_name = context
            .get_attribute("filename")?
            .unwrap_or(context.get_id()?);
        Ok(directory.join(file_name))
    }

    fn prepare_destination(&self, destination: &Path) -> std::io::Result<()> {
        if let Some(parent) = destination.parent()
            && self.try_make_dirs
        {
            std::fs::create_dir_all(parent)?;
            self.unix_permissions.set_directory_permissions(parent)?;
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
        let file_permissions = context.get_property(&unix_only_properties::PERMISSIONS)?;
        let directory_permissions =
            context.get_property(&unix_only_properties::DIRECTORY_PERMISSIONS)?;

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
        let conflict_resolution_strategy =
            context.get_property(&properties::CONFLICT_RESOLUTION)?;

        let try_make_dirs = context.get_property(&properties::CREATE_DIRS)?;

        let maximum_file_count = context.get_property(&properties::MAX_FILE_COUNT)?;

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
        Context: GetProperty + GetControllerService + GetAttribute + GetId,
        LoggerImpl: Logger,
    >(
        &self,
        context: &Context,
        input_stream: &'a mut dyn InputStream,
        logger: &LoggerImpl,
    ) -> Result<TransformedFlowFile<'a>, ProcessError> {
        trace!(logger, "on_trigger: {:?}", self);

        let destination_path = Self::get_destination_path(context).route_err_to_failure()?;

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

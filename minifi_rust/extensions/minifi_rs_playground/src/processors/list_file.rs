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

// A (not production ready) reimplementation of the standard ListFile processor, doubling as the proof-of-use for
// the persistent state API.

use crate::processors::list_file::output_attributes::{
    ABSOLUTE_PATH_OUTPUT_ATTRIBUTE, FILE_SIZE_OUTPUT_ATTRIBUTE, FILENAME_OUTPUT_ATTRIBUTE,
};
use crate::processors::list_file::properties::{DIRECTORY, IGNORE_HIDDEN_FILES, RECURSE};
use minifi_native::macros::{ComponentIdentifier, StateSchema};
use minifi_native::{
    GeneratedFlowFile, GetControllerService, GetProperty, ListingState, Logger, MinifiError,
    MutFlowFileSource, Schedule, debug, trace, warn,
};
use std::path::PathBuf;
use std::time::UNIX_EPOCH;
use walkdir::{DirEntry, WalkDir};

mod output_attributes;
pub(crate) mod processor_definition;
mod properties;
mod relationships;

#[derive(Debug, ComponentIdentifier, StateSchema)]
pub(crate) struct ListFileRs {
    input_directory: PathBuf,
    recursive: bool,
    ignore_hidden_files: bool,
    // `flatten` because the listing spreads itself over several entries, in the layout C++ ListFile writes.
    #[persisted(flatten)]
    listing_state: ListingState,
}

struct ListedFile {
    absolute_path: String,
    file_name: String,
    size: u64,
    modified_ms: i64,
}

impl ListFileRs {
    fn is_hidden(entry: &DirEntry) -> bool {
        entry
            .file_name()
            .to_str()
            .is_some_and(|name| name.starts_with('.'))
    }

    fn discover_files<L: Logger>(&self, logger: &L) -> Vec<ListedFile> {
        let mut walker = WalkDir::new(&self.input_directory);
        if !self.recursive {
            walker = walker.max_depth(1);
        }

        let mut files = Vec::new();
        for entry in walker.into_iter().filter_map(Result::ok) {
            if self.ignore_hidden_files && Self::is_hidden(&entry) {
                continue;
            }
            let metadata = match entry.metadata() {
                Ok(metadata) if metadata.is_file() => metadata,
                Ok(_) => continue,
                Err(err) => {
                    warn!(logger, "Could not stat {:?}: {}", entry.path(), err);
                    continue;
                }
            };
            let modified_ms = metadata
                .modified()
                .ok()
                .and_then(|time| time.duration_since(UNIX_EPOCH).ok())
                .map(|elapsed| elapsed.as_millis() as i64)
                .unwrap_or(0);
            files.push(ListedFile {
                absolute_path: entry.path().to_string_lossy().into_owned(),
                file_name: entry.file_name().to_string_lossy().into_owned(),
                size: metadata.len(),
                modified_ms,
            });
        }
        files
    }
}

impl Schedule for ListFileRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let input_directory: PathBuf = context.get_property(&DIRECTORY)?;
        if !input_directory.is_dir() {
            return Err(MinifiError::custom(format!(
                "{:?} is not a valid directory",
                input_directory
            )));
        }
        Ok(ListFileRs {
            input_directory,
            recursive: context.get_property(&RECURSE)?,
            ignore_hidden_files: context.get_property(&IGNORE_HIDDEN_FILES)?,
            listing_state: ListingState::default(),
        })
    }
}

impl MutFlowFileSource for ListFileRs {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &mut self,
        _context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, MinifiError> {
        let discovered = self.discover_files(logger);

        // Decide "new" against a snapshot: updating the same state mid-pass would advance the watermark while
        // iterating and wrongly skip older files found later. C++ ListFile takes the same snapshot.
        let stored_state = self.listing_state.clone();

        let mut generated = Vec::new();
        for file in &discovered {
            if stored_state.was_listed_already(&file.absolute_path, file.modified_ms) {
                continue;
            }
            trace!(logger, "Listing new file {}", file.absolute_path);
            generated.push(
                GeneratedFlowFile::new(&relationships::SUCCESS, None)
                    .with_attribute(FILENAME_OUTPUT_ATTRIBUTE.name, file.file_name.clone())
                    .with_attribute(
                        ABSOLUTE_PATH_OUTPUT_ATTRIBUTE.name,
                        file.absolute_path.clone(),
                    )
                    .with_attribute(FILE_SIZE_OUTPUT_ATTRIBUTE.name, file.size.to_string()),
            );
            self.listing_state
                .update(&file.absolute_path, file.modified_ms);
        }

        debug!(
            logger,
            "ListFile listed {} new file(s) out of {} discovered",
            generated.len(),
            discovered.len()
        );
        Ok(generated)
    }
}

#[cfg(test)]
mod tests;

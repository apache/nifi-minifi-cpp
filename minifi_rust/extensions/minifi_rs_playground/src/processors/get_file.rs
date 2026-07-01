use crate::processors::get_file::output_attributes::{
    ABSOLUTE_PATH_OUTPUT_ATTRIBUTE, FILENAME_OUTPUT_ATTRIBUTE,
};
use crate::processors::get_file::properties::{
    BATCH_SIZE, DIRECTORY, IGNORE_HIDDEN_FILES, KEEP_SOURCE_FILE, MAX_AGE, MAX_SIZE, MIN_AGE,
    MIN_SIZE, RECURSE,
};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    GetProperty, IoState, Logger, MinifiError, OnTriggerResult, ProcessContext, ProcessSession,
    Schedule, Trigger, debug, info, trace, warn,
};
use std::collections::VecDeque;
use std::error;
use std::fs::File;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::{Duration, Instant, SystemTime};
use walkdir::{DirEntry, WalkDir};

mod properties;
mod relationships;

#[derive(Debug)]
struct GetFileMetrics {
    accepted_files: u32,
    input_bytes: u64,
}

#[derive(Debug)]
struct DirectoryListing {
    paths: VecDeque<PathBuf>,
    last_polling_time: Option<Instant>,
}

impl DirectoryListing {
    fn new() -> Self {
        Self {
            paths: VecDeque::new(),
            last_polling_time: None,
        }
    }
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct GetFileRs {
    recursive: bool,
    keep_source_file: bool,
    input_directory: PathBuf,
    poll_interval: Option<Duration>,
    directory_listing: Mutex<DirectoryListing>,
    batch_size: u64,
    min_size: Option<u64>,
    max_size: Option<u64>,
    min_age: Option<Duration>,
    max_age: Option<Duration>,
    ignore_hidden_files: bool,
    metrics: Mutex<GetFileMetrics>,
}

impl GetFileRs {
    fn is_listing_empty(&self) -> bool {
        let directory_listing = self.directory_listing.lock().unwrap();
        directory_listing.paths.is_empty()
    }

    fn poll_listing(&self, batch_size: u64) -> VecDeque<PathBuf> {
        let mut directory_listings = self.directory_listing.lock().unwrap();

        let mut res = VecDeque::new();
        for _ in 0..batch_size {
            if let Some(path) = directory_listings.paths.pop_back() {
                res.push_back(path);
            } else {
                break;
            }
        }

        res
    }

    fn should_poll(&self) -> bool {
        if self.poll_interval.is_none() {
            return true;
        }
        let directory_listings = self.directory_listing.lock().unwrap();

        if directory_listings.last_polling_time.is_none() {
            return true;
        }
        Instant::now() - directory_listings.last_polling_time.unwrap() > self.poll_interval.unwrap()
    }

    fn perform_listing(&self) {
        let mut walker = WalkDir::new(&self.input_directory);
        if !self.recursive {
            walker = walker.max_depth(1);
        }

        let mut new_paths: VecDeque<PathBuf> = VecDeque::new();
        let mut files_added = 0u32;
        let mut bytes_added = 0u64;
        for entry in walker.into_iter().filter_map(Result::ok) {
            if self.entry_matches_criteria(&entry).unwrap_or(false) {
                let file_size = entry.metadata().map(|m| m.len()).unwrap_or(0);
                new_paths.push_back(entry.into_path());
                files_added += 1;
                bytes_added += file_size;
            }
        }

        {
            let mut directory_listings = self.directory_listing.lock().unwrap();
            directory_listings.paths.extend(new_paths);
            directory_listings.last_polling_time = Some(Instant::now());
        }

        let mut metrics = self.metrics.lock().unwrap();
        metrics.accepted_files += files_added;
        metrics.input_bytes += bytes_added;
    }

    fn entry_matches_criteria(&self, dir_entry: &DirEntry) -> Result<bool, Box<dyn error::Error>> {
        let metadata = dir_entry.metadata()?;
        if !metadata.is_file() {
            return Ok(false);
        }
        let age = SystemTime::now().duration_since(metadata.modified()?)?;
        let size = metadata.len();

        if self.min_age.is_some() && age < self.min_age.unwrap() {
            return Ok(false);
        }
        if self.max_age.is_some() && age > self.max_age.unwrap() {
            return Ok(false);
        }
        if self.min_size.is_some() && size < self.min_size.unwrap() {
            return Ok(false);
        }
        if self.max_size.is_some() && size > self.max_size.unwrap() {
            return Ok(false);
        }

        fn is_hidden(path: PathBuf) -> bool {
            path.file_name()
                .and_then(|f| f.to_str())
                .is_some_and(|f| f.starts_with('.'))
        }

        if self.ignore_hidden_files && is_hidden(dir_entry.path().to_path_buf()) {
            return Ok(false);
        }

        Ok(true)
    }

    fn get_single_file<PS: ProcessSession, L: Logger>(
        &self,
        session: &mut PS,
        logger: &L,
        path: &Path,
    ) -> Result<(), MinifiError> {
        info!(logger, "GetFile process {:?}", path);
        let mut ff = session
            .create()
            .expect("Successful FlowFile creation is expected");

        if let Some(file_name) = path.file_name().and_then(|f| f.to_str()) {
            session.set_attribute(&mut ff, FILENAME_OUTPUT_ATTRIBUTE.name, file_name)?;
        } else {
            warn!(logger, "Couldnt get filename of {:?}", path);
        }
        session.set_attribute(
            &mut ff,
            ABSOLUTE_PATH_OUTPUT_ATTRIBUTE.name,
            path.to_string_lossy().trim(),
        )?;

        session.write_stream(&ff, |output_stream| {
            let mut file = File::open(path)?;
            std::io::copy(&mut file, output_stream)?;
            Ok(((), IoState::Ok))
        })?;
        if !self.keep_source_file
            && let Err(err) = std::fs::remove_file(path)
        {
            warn!(logger, "Failed to remove source file {:?}", err);
        }
        session.transfer(ff, relationships::SUCCESS.name)?;
        Ok(())
    }

    fn calculate_metrics(&self) -> Vec<(String, f64)> {
        let metrics = self.metrics.lock().unwrap();
        vec![
            ("accepted_files".to_string(), metrics.accepted_files as f64),
            ("input_bytes".to_string(), metrics.input_bytes as f64),
        ]
    }
}

impl Schedule for GetFileRs {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let input_directory: PathBuf = context
            .get_property(&DIRECTORY)?
            .expect("Required property")
            .into();
        if !input_directory.is_dir() {
            return Err(MinifiError::schedule_err(format!(
                "{:?} is not a valid directory",
                input_directory
            )));
        }

        let recursive = context
            .get_bool_property(&RECURSE)?
            .expect("Required property");

        let keep_source_file = context
            .get_bool_property(&KEEP_SOURCE_FILE)?
            .expect("Required property");

        let poll_interval = context.get_duration_property(&properties::POLLING_INTERVAL)?;
        let min_size = context.get_size_property(&MIN_SIZE)?;
        let max_size = context.get_size_property(&MAX_SIZE)?;
        let min_age = context.get_duration_property(&MIN_AGE)?;
        let max_age = context.get_duration_property(&MAX_AGE)?;
        let batch_size = context
            .get_u64_property(&BATCH_SIZE)?
            .expect("required property");
        let ignore_hidden_files = context
            .get_bool_property(&IGNORE_HIDDEN_FILES)?
            .expect("required property");

        Ok(GetFileRs {
            recursive,
            keep_source_file,
            input_directory,
            poll_interval,
            directory_listing: Mutex::new(DirectoryListing::new()),
            batch_size,
            min_size,
            max_size,
            min_age,
            max_age,
            ignore_hidden_files,
            metrics: Mutex::new(GetFileMetrics {
                accepted_files: 0,
                input_bytes: 0,
            }),
        })
    }
}

impl Trigger for GetFileRs {
    fn trigger<PC, PS, L>(
        &self,
        context: &mut PC,
        session: &mut PS,
        logger: &L,
    ) -> Result<OnTriggerResult, MinifiError>
    where
        PC: ProcessContext,
        PS: ProcessSession<FlowFile = PC::FlowFile>,
        L: Logger,
    {
        trace!(logger, "on_trigger: {:?}", self);
        {
            let is_dir_empty_before_poll = self.is_listing_empty();
            debug!(
                logger,
                "Listing is {} before polling directory", is_dir_empty_before_poll
            );
            if is_dir_empty_before_poll && self.should_poll() {
                self.perform_listing();
            }
        }
        {
            let is_dir_empty_after_poll = self.is_listing_empty();
            debug!(
                logger,
                "Listing is {} after polling directory", is_dir_empty_after_poll
            );
            if is_dir_empty_after_poll {
                return Ok(OnTriggerResult::Yield);
            }
        }

        let files = self.poll_listing(self.batch_size);
        for file in files {
            // A single mid-batch failure (deleted, permission flipped,
            // transient I/O) should not orphan the flow files already
            // transferred earlier in the same trigger. Log and continue.
            if let Err(err) = self.get_single_file(session, logger, &file) {
                warn!(logger, "Failed to ingest {:?}: {}", file, err);
            }
        }
        context.report_metrics(self.calculate_metrics())?;
        Ok(OnTriggerResult::Ok)
    }
}

pub(crate) mod processor_definition;

mod output_attributes;
#[cfg(test)]
mod tests;

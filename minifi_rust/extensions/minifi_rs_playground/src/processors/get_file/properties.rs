// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use minifi_native::{DataSize, NonBlankPath, Property};
use std::time::Duration;

pub(crate) const DIRECTORY: Property<NonBlankPath> = Property::new(
    "Input Directory",
    "The input directory from which to pull files",
)
.supports_expression_language();

pub(crate) const RECURSE: Property<bool> = Property::new(
    "Recurse Subdirectories",
    "Indicates whether or not to pull files from subdirectories",
)
.with_default("true");

pub(crate) const KEEP_SOURCE_FILE: Property<bool> = Property::new(
    "Keep Source File",
    "If true, the file is not deleted after it has been copied to the Content Repository",
)
.with_default("false");

pub(crate) const MIN_AGE: Property<Option<Duration>> = Property::new(
    "Minimum File Age",
    "The minimum age that a file must be in order to be pulled; any file younger than this amount of time (according to last modification date) will be ignored",
);

pub(crate) const MAX_AGE: Property<Option<Duration>> = Property::new(
    "Maximum File Age",
    "The maximum age that a file must be in order to be pulled;  any file older than this amount of time (according to last modification date) will be ignored",
);

pub(crate) const MIN_SIZE: Property<Option<DataSize>> = Property::new(
    "Minimum File Size",
    "The minimum size that a file can be in order to be pulled",
);

pub(crate) const MAX_SIZE: Property<Option<DataSize>> = Property::new(
    "Maximum File Size",
    "The maximum size that a file can be in order to be pulled",
);

pub(crate) const IGNORE_HIDDEN_FILES: Property<bool> = Property::new(
    "Ignore Hidden Files",
    "Indicates whether or not hidden files should be ignored",
)
.with_default("true");

pub(crate) const POLLING_INTERVAL: Property<Option<Duration>> = Property::new(
    "Polling Interval",
    "Indicates how long to wait before performing a directory listing",
);

pub(crate) const BATCH_SIZE: Property<u64> = Property::new(
    "Batch Size",
    "The maximum number of files to pull in each iteration",
)
.with_default("10");

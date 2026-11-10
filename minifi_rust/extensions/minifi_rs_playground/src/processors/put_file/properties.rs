// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::ConflictResolutionStrategy;
use minifi_native::{NonBlankPath, Property};

pub(crate) const DIRECTORY: Property<NonBlankPath> =
    Property::new("Directory", "The output directory to which to put files")
        .supports_expression_language()
        .with_default(".");

pub(crate) const CONFLICT_RESOLUTION: Property<ConflictResolutionStrategy> = Property::new(
    "Conflict Resolution Strategy",
    "Indicates what should happen when a file with the same name already exists in the output directory",
)
.with_default(ConflictResolutionStrategy::Fail.into_str());

pub(crate) const CREATE_DIRS: Property<bool> = Property::new(
    "Create Missing Directories",
    "If true, then missing destination directories will be created. If false, flowfiles are penalized and sent to failure.",
)
.with_default("true");

pub(crate) const MAX_FILE_COUNT: Property<Option<u64>> = Property::new(
    "Maximum File Count",
    "Specifies the maximum number of files that can exist in the output directory",
);

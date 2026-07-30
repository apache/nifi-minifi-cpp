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

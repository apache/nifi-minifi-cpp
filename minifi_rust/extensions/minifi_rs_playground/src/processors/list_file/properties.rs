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

use minifi_native::{NonBlankPath, Property};

pub(crate) const DIRECTORY: Property<NonBlankPath> = Property::new(
    "Input Directory",
    "The input directory from which files are listed",
)
.supports_expression_language();

pub(crate) const RECURSE: Property<bool> = Property::new(
    "Recurse Subdirectories",
    "Indicates whether or not to list files from subdirectories of the directory",
)
.with_default("true");

pub(crate) const IGNORE_HIDDEN_FILES: Property<bool> = Property::new(
    "Ignore Hidden Files",
    "Indicates whether or not hidden files should be ignored",
)
.with_default("true");

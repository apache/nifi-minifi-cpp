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

use minifi_native::{Property, StandardPropertyValidator};

pub(crate) const PERMISSIONS: Property = Property {
    name: "Permissions",
    description: "Sets the permissions on the output file to the value of this attribute. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

pub(crate) const DIRECTORY_PERMISSIONS: Property = Property {
    name: "Directory Permissions",
    description: "Sets the permissions on the directories being created if 'Create Missing Directories' property is set. Must be an octal number (e.g. 644 or 0755). Not supported on Windows systems.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

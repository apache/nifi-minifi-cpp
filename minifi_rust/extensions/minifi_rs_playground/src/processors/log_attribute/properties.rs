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

use minifi_native::{LogLevel, Property, property_constraint};

pub(crate) const LOG_LEVEL: Property = Property {
    name: "Log Level",
    description: "The Log Level to use when logging the Attributes",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(LogLevel::Info.into_str()),
    constraints: property_constraint::<LogLevel>(),
};

pub(crate) const ATTRIBUTES_TO_LOG: Property = Property {
    name: "Attributes to Log",
    description: "A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: None,
};

pub(crate) const ATTRIBUTES_TO_IGNORE: Property = Property {
    name: "Attributes to Ignore",
    description: "A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: None,
};

pub(crate) const LOG_PAYLOAD: Property = Property {
    name: "Log Payload",
    description: "If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    constraints: property_constraint::<bool>(),
};

pub(crate) const LOG_PREFIX: Property = Property {
    name: "Log Prefix",
    description: "Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: None,
};

pub(crate) const FLOW_FILES_TO_LOG: Property = Property {
    name: "FlowFiles To Log",
    description: "Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1"),
    constraints: property_constraint::<u64>(),
};

pub(crate) const HEX_ENCODE_PAYLOAD: Property = Property {
    name: "Hexencode Payload",
    description: "If true, the FlowFile's payload will be logged in a hexencoded format",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    constraints: property_constraint::<bool>(),
};

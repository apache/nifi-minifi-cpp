// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::processors::log_attribute::AttributeList;
use minifi_native::{LogLevel, Property};

pub(crate) const LOG_LEVEL: Property<LogLevel> = Property::new(
    "Log Level",
    "The Log Level to use when logging the Attributes",
)
.with_default(LogLevel::Info.into_str());

pub(crate) const ATTRIBUTES_TO_LOG: Property<Option<AttributeList>> = Property::new(
    "Attributes to Log",
    "A comma-separated list of Attributes to Log. If not specified, all attributes will be logged.",
);

pub(crate) const ATTRIBUTES_TO_IGNORE: Property<Option<AttributeList>> = Property::new(
    "Attributes to Ignore",
    "A comma-separated list of Attributes to ignore. If not specified, no attributes will be ignored.",
);

pub(crate) const LOG_PAYLOAD: Property<bool> = Property::new(
    "Log Payload",
    "If true, the FlowFile's payload will be logged, in addition to its attributes. Otherwise, just the Attributes will be logged.",
)
.with_default("false");

pub(crate) const LOG_PREFIX: Property<Option<String>> = Property::new(
    "Log Prefix",
    "Log prefix appended to the log lines. It helps to distinguish the output of multiple LogAttribute processors.",
);

pub(crate) const FLOW_FILES_TO_LOG: Property<usize> = Property::new(
    "FlowFiles To Log",
    "Number of flow files to log. If set to zero all flow files will be logged. Please note that this may block other threads from running if not used judiciously.",
)
.with_default("1");

pub(crate) const HEX_ENCODE_PAYLOAD: Property<bool> = Property::new(
    "Hexencode Payload",
    "If true, the FlowFile's payload will be logged in a hexencoded format",
)
.with_default("false");

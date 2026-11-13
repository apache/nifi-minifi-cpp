// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::DataFormat;
use minifi_native::{DataSize, Property};

pub(crate) const FILE_SIZE: Property<DataSize> =
    Property::new("File Size", "The size of the file that will be used")
        .supports_expression_language()
        .with_default("1 kB");

pub(crate) const BATCH_SIZE: Property<u64> = Property::new(
    "Batch Size",
    "The number of FlowFiles to be transferred in each invocation",
)
.with_default("1");

pub(crate) const DATA_FORMAT: Property<DataFormat> = Property::new(
    "Data Format",
    "Specifies whether the data should be Text or Binary",
)
.with_default(DataFormat::Binary.into_str());

pub(crate) const UNIQUE_FLOW_FILES: Property<bool> = Property::new(
    "Unique FlowFiles",
    "If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput (but see the description of Custom Text for special non-random use cases)",
)
.with_default("true");

pub(crate) const CUSTOM_TEXT: Property<Option<String>> = Property::new(
    "Custom Text",
    "If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles",
)
.supports_expression_language();

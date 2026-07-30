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

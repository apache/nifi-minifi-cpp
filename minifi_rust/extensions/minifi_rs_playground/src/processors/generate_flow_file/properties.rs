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

use minifi_native::PropertyConstraints::AllowedValues;
use minifi_native::{DataSize, Property, property_constraint};

pub(crate) const FILE_SIZE: Property = Property {
    name: "File Size",
    description: "The size of the file that will be used",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: Some("1 kB"),
    constraints: property_constraint::<DataSize>(),
};

pub(crate) const BATCH_SIZE: Property = Property {
    name: "Batch Size",
    description: "The number of FlowFiles to be transferred in each invocation",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("1"),
    constraints: property_constraint::<u64>(),
};

pub(crate) const DATA_FORMAT: Property = Property {
    name: "Data Format",
    description: "Specifies whether the data should be Text or Binary",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("Binary"),
    constraints: Some(AllowedValues(&["Text", "Binary"])),
};

pub(crate) const UNIQUE_FLOW_FILES: Property = Property {
    name: "Unique FlowFiles",
    description: "If true, each FlowFile that is generated will be unique. If false, a random value will be generated and all FlowFiles will get the same content but this offers much higher throughput (but see the description of Custom Text for special non-random use cases)",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("true"),
    constraints: property_constraint::<bool>(),
};

pub(crate) const CUSTOM_TEXT: Property = Property {
    name: "Custom Text",
    description: "If Data Format is text and if Unique FlowFiles is false, then this custom text will be used as content of the generated FlowFiles and the File Size will be ignored. Finally, if Expression Language is used, evaluation will be performed only once per batch of generated FlowFiles",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: true,
    default_value: None,
    constraints: None,
};

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

use super::{EncryptContentPGP, FileEncoding};
use crate::controller_services::public_key_service::PGPPublicKeyService;
use crate::utils;
use minifi_native::{
    OutputAttribute, ProcessorDefinition, ProcessorInputRequirement, Property, PropertyDefinition,
    Relationship, property_definitions,
};

pub(crate) const FILE_ENCODING: Property<FileEncoding> =
    Property::new("File Encoding", "File Encoding for encryption")
        .with_default(FileEncoding::Binary.into_str());
pub(crate) const PASSWORD: Property<Option<utils::Password>> = Property::new(
    "Symmetric Password",
    "Password used for encrypting data with Password-Based Encryption",
)
.sensitive();

pub(crate) const PUBLIC_KEY_SEARCH: Property<Option<String>> = Property::new(
    "Public Key Search",
    "PGP Public Key Search will be used to match against the User ID or Key ID when formatted as uppercase hexadecimal string of 16 characters",
).supports_expression_language();

pub(crate) const PUBLIC_KEY_SERVICE: Property<Option<PGPPublicKeyService>> = Property::new(
    "Public Key Service",
    "PGP Public Key Service for encrypting data with Public Key Encryption",
);

pub(super) const FILE_ENCODING_ATTR: OutputAttribute = OutputAttribute {
    name: "pgp.file.encoding",
    relationships: &["success"],
    description: "File Encoding",
};

pub(super) const SUCCESS: Relationship = Relationship {
    name: "success",
    description: "Encryption Succeeded",
};

pub(super) const FAILURE: Relationship = Relationship {
    name: "failure",
    description: "Encryption Failed",
};

impl ProcessorDefinition for EncryptContentPGP {
    const DESCRIPTION: &'static str = "Encrypt contents using OpenPGP.";
    const INPUT_REQUIREMENT: ProcessorInputRequirement = ProcessorInputRequirement::Required;
    const SUPPORTS_DYNAMIC_PROPERTIES: bool = false;
    const SUPPORTS_DYNAMIC_RELATIONSHIPS: bool = false;
    const OUTPUT_ATTRIBUTES: &'static [OutputAttribute] = &[FILE_ENCODING_ATTR];
    const RELATIONSHIPS: &'static [Relationship] = &[SUCCESS, FAILURE];

    const PROPERTIES: &[PropertyDefinition] = property_definitions![
        FILE_ENCODING,
        PASSWORD,
        PUBLIC_KEY_SEARCH,
        PUBLIC_KEY_SERVICE,
    ];
}

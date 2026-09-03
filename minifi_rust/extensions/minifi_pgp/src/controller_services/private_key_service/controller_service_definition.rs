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

use super::PGPPrivateKeyService;
use crate::controller_services::key_file_property::SecretKeyFile;
use crate::controller_services::key_property::SecretKey;
use crate::utils;
use minifi_native::{
    ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
    property_definitions,
};

pub(super) const KEY_FILE: Property<Option<SecretKeyFile>> = Property::new(
    "Key File",
    "File path to PGP Secret Key encoded in binary or ASCII Armor",
)
.supports_expression_language();

pub(super) const KEY: Property<Option<SecretKey>> =
    Property::new("Key", "Secret Key encoded in ASCII Armor").sensitive();

pub(super) const KEY_PASSPHRASE: Property<Option<utils::Password>> = Property::new(
    "Key Passphrase",
    "Passphrase used for decrypting Private Keys",
)
.sensitive();

impl ControllerServiceDefinition for PGPPrivateKeyService {
    const DESCRIPTION: &'static str =
        "PGP Private Key Service provides Private Keys loaded from files or properties";
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![KEY_FILE, KEY, KEY_PASSPHRASE];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}

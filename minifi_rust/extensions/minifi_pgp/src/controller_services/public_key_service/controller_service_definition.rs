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

use super::PGPPublicKeyService;
use crate::controller_services::key_file_property::PublicKeyFile;
use crate::controller_services::key_property::PublicKey;
use minifi_native::{
    ControllerServiceDefinition, Property, PropertyDefinition, ProvidedInterface,
    property_definitions,
};

pub(crate) const KEYRING_FILE: Property<Option<PublicKeyFile>> = Property::new(
    "Keyring File",
    "File path to PGP Keyring or Public Key encoded in binary or ASCII Armor",
)
.supports_expression_language();

pub(crate) const KEYRING: Property<Option<PublicKey>> = Property::new(
    "Keyring",
    "PGP Keyring or Public Key encoded in ASCII Armor",
)
.sensitive();

impl ControllerServiceDefinition for PGPPublicKeyService {
    const DESCRIPTION: &'static str =
        "PGP Public Key Service providing Public Keys loaded from files";
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![KEYRING_FILE, KEYRING];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}

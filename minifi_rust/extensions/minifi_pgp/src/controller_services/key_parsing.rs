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

use minifi_native::{GetProperty, MinifiError, Property, PropertyType};
use pgp::composed::Deserializable;

/// Describes which flavour of key is being loaded, used for error messages only.
#[derive(Copy, Clone)]
pub(crate) enum KeyKind {
    Public,
    Secret,
}

impl KeyKind {
    fn no_valid_keys(self) -> MinifiError {
        match self {
            KeyKind::Public => MinifiError::validation("Couldn't load any valid public keys"),
            KeyKind::Secret => MinifiError::validation("Couldn't load any valid secret keys"),
        }
    }
}

/// Parses every key found in ASCII Armored `input`.
pub(crate) fn parse_armored_keys<T: Deserializable>(
    input: &str,
    kind: KeyKind,
) -> Result<Vec<T>, MinifiError> {
    let mut keys: Vec<T> = Vec::new();
    if let Ok((parsed, _headers)) = T::from_armor_many(input.as_bytes()) {
        keys.extend(parsed.filter_map(Result::ok));
    }
    non_empty(keys, kind)
}

/// Parses every key found in the file at `path`, which may be ASCII Armored or binary.
pub(crate) fn parse_key_file<T: Deserializable>(
    path: &str,
    kind: KeyKind,
) -> Result<Vec<T>, MinifiError> {
    let mut keys: Vec<T> = Vec::new();
    if let Ok((parsed, _headers)) = T::from_armor_file_many(path) {
        keys.extend(parsed.filter_map(Result::ok));
    } else if let Ok(parsed) = T::from_file_many(path) {
        keys.extend(parsed.filter_map(Result::ok));
    }
    non_empty(keys, kind)
}

/// Loads the keys of a controller service from its file property and its inline property,
/// failing when neither yields a usable key.
pub(crate) fn load_service_keys<T, File, Inline, Ctx>(
    context: &Ctx,
    file_property: &Property<Option<File>>,
    inline_property: &Property<Option<Inline>>,
) -> Result<Vec<T>, MinifiError>
where
    Ctx: GetProperty,
    File: PropertyType<Output = Vec<T>>,
    Inline: PropertyType<Output = Vec<T>>,
{
    let mut keys = context.get_property(file_property)?.unwrap_or_default();
    keys.extend(context.get_property(inline_property)?.unwrap_or_default());

    if keys.is_empty() {
        return Err(MinifiError::validation("Could not load any valid keys"));
    }
    Ok(keys)
}

fn non_empty<T>(keys: Vec<T>, kind: KeyKind) -> Result<Vec<T>, MinifiError> {
    if keys.is_empty() {
        Err(kind.no_valid_keys())
    } else {
        Ok(keys)
    }
}

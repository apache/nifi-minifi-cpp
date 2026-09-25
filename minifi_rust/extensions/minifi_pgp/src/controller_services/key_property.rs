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

use crate::controller_services::key_parsing::{KeyKind, parse_armored_keys};
use minifi_native::{MinifiError, PropertyConstraints, PropertySchema, PropertyType};
use pgp::composed::{SignedPublicKey, SignedSecretKey};

pub(crate) struct SecretKey {}

impl PropertySchema for SecretKey {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = true;
}

impl PropertyType for SecretKey {
    type Output = Vec<SignedSecretKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        parse_armored_keys(s, KeyKind::Secret)
    }
}

pub(crate) struct PublicKey {}
impl PropertySchema for PublicKey {
    const CONSTRAINT: Option<PropertyConstraints> = None;
    const IS_REQUIRED: bool = true;
}

impl PropertyType for PublicKey {
    type Output = Vec<SignedPublicKey>;

    fn parse(s: &str) -> Result<Self::Output, MinifiError> {
        parse_armored_keys(s, KeyKind::Public)
    }
}

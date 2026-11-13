// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

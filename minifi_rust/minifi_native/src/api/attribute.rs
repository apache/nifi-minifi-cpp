// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::MinifiError;
use std::borrow::Cow;

pub struct OutputAttribute {
    pub name: &'static str,
    pub relationships: &'static [&'static str],
    pub description: &'static str,
}

impl From<&OutputAttribute> for Cow<'static, str> {
    fn from(attr: &OutputAttribute) -> Self {
        Cow::Borrowed(attr.name)
    }
}

pub trait GetAttribute {
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError>;
    fn get_required_attribute(&self, name: &str) -> Result<String, MinifiError> {
        self.get_attribute(name)?
            .ok_or(MinifiError::MissingRequiredAttribute(
                name.to_owned().into(),
            ))
    }
}

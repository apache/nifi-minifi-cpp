// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::property::PropertySchema;
use crate::mock::mock_process_context::MockPropertyMap;
use crate::{GetProperty, MinifiError, Property};

pub struct MockControllerServiceContext {
    pub properties: MockPropertyMap,
}

impl GetProperty for MockControllerServiceContext {
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, None)
    }
}

impl Default for MockControllerServiceContext {
    fn default() -> Self {
        Self::new()
    }
}

impl MockControllerServiceContext {
    pub fn new() -> Self {
        Self {
            properties: MockPropertyMap::new(),
        }
    }
}

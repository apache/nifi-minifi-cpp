// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use super::mock_configuration::{MockControllerServices, MockPropertyMap, find_controller_service};
use crate::GetProperty;
use crate::api::property::PropertySchema;
use crate::api::{GetId, RawControllerService, ScheduleContext};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, GetAttribute, MinifiError,
    Property,
};
use std::collections::HashMap;

pub struct MockScheduleContext {
    pub properties: MockPropertyMap,
    pub controller_services: MockControllerServices,
    pub attributes: HashMap<String, String>,
}

impl GetProperty for MockScheduleContext {
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, None)
    }
}

impl ScheduleContext for MockScheduleContext {
    fn get_raw_controller_service<Cs, P>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static,
        P: PropertySchema + ?Sized,
    {
        find_controller_service(&self.properties, &self.controller_services, property)
    }

    fn get_controller_service<Cs>(
        &self,
        property: &Property<Cs>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static,
    {
        find_controller_service(&self.properties, &self.controller_services, property)
    }

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi + PropertySchema>(
        &self,
        _property: &Property<Trait>,
    ) -> Result<Option<Box<&Trait>>, MinifiError> {
        // Interface lookups are not mockable (no 'static / Any on the trait object), so none is ever found.
        Ok(None)
    }
}

impl Default for MockScheduleContext {
    fn default() -> Self {
        Self::new()
    }
}

impl MockScheduleContext {
    pub fn new() -> Self {
        Self {
            properties: MockPropertyMap::new(),
            controller_services: HashMap::new(),
            attributes: HashMap::new(),
        }
    }
}

impl GetAttribute for MockScheduleContext {
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError> {
        Ok(self.attributes.get(name).cloned())
    }
}

impl GetId for MockScheduleContext {
    fn get_id(&self) -> Result<String, MinifiError> {
        Ok("mock_flow_file_id".into())
    }
}

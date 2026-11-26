// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::property::PropertySchema;
use crate::{ComponentIdentifier, MinifiError, MockFlowFile, Property};
use std::any::Any;
use std::collections::HashMap;

pub type MockControllerServices = HashMap<String, Box<dyn Any>>;

pub struct MockPropertyMap {
    pub properties: HashMap<String, String>,
}

impl MockPropertyMap {
    pub fn new() -> Self {
        Self {
            properties: HashMap::new(),
        }
    }

    pub fn insert<K, V>(&mut self, key: K, value: V)
    where
        K: Into<String>,
        V: Into<String>,
    {
        self.properties.insert(key.into(), value.into());
    }

    pub fn extend<I, K, V>(&mut self, iter: I)
    where
        I: IntoIterator<Item = (K, V)>,
        K: Into<String>,
        V: Into<String>,
    {
        self.properties
            .extend(iter.into_iter().map(|(k, v)| (k.into(), v.into())))
    }
}

impl MockPropertyMap {
    pub fn get_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
        _flow_file: Option<&MockFlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        if let Some(value) = self.properties.get(property.name) {
            Ok(Some(value.clone()))
        } else {
            Ok(property
                .default_value
                .map(|default_val| default_val.to_string()))
        }
    }
}

pub fn find_controller_service<'a, Cs, P>(
    properties: &MockPropertyMap,
    controller_services: &'a MockControllerServices,
    property: &Property<P>,
) -> Result<Option<&'a Cs>, MinifiError>
where
    Cs: ComponentIdentifier + 'static,
    P: PropertySchema + ?Sized,
{
    Ok(properties
        .get_property(property, None)?
        .and_then(|service_name| controller_services.get(&service_name))
        .and_then(|service| service.downcast_ref::<Cs>()))
}

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

use crate::api::property::PropertySchema;
use crate::api::{GetId, ProcessContext, RawControllerService};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, GetAttribute, MinifiError,
    MockFlowFile, Property,
};
use std::any::Any;
use std::collections::HashMap;

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
    pub fn get_property<K: PropertySchema + ?Sized>(
        &self,
        property: &Property<K>,
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

pub struct MockProcessContext {
    pub properties: MockPropertyMap,
    pub controller_services: HashMap<String, Box<dyn Any>>,
    pub attributes: HashMap<String, String>,
}

impl ProcessContext for MockProcessContext {
    type FlowFile = MockFlowFile;

    fn get_raw_property<K: PropertySchema + ?Sized>(
        &self,
        property: &Property<K>,
        _flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, _flow_file)
    }

    fn get_raw_controller_service<Cs, K>(
        &self,
        property: &Property<K>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static,
        K: PropertySchema + ?Sized,
    {
        // Mirror `get_controller_service`: resolve the property to a
        // service name and downcast the registered `Box<dyn Any>`.
        if let Some(service_name) = self.get_raw_property(property, None)? {
            Ok(self
                .controller_services
                .get(&service_name)
                .and_then(|c| c.downcast_ref::<Cs>()))
        } else {
            Ok(None)
        }
    }

    fn get_controller_service<Cs>(
        &self,
        property: &Property<Cs>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static,
    {
        if let Some(service_name) = self.get_raw_property(property, None)? {
            Ok(self
                .controller_services
                .get(&service_name)
                .and_then(|c| c.downcast_ref::<Cs>()))
        } else {
            Ok(None)
        }
    }

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi + PropertySchema>(
        &self,
        _property: &Property<Trait>,
    ) -> Result<Option<Box<&Trait>>, MinifiError> {
        // A fully-typed mock for `dyn Trait` interfaces would need per-property
        // registration keyed by both property name and interface name; the
        // combination of `?Sized + ControllerServiceApi` (no `'static`, no
        // `Any`) makes safe storage awkward. Return `None` by default so
        // processors that call this method can still be scheduled and
        // triggered under the mock — tests that need a live implementation
        // should exercise it in the FFI path via `cargo behave`.
        Ok(None)
    }

    fn report_metrics(&self, _metrics: Vec<(String, f64)>) -> Result<(), MinifiError> {
        Ok(())
    }
}

impl Default for MockProcessContext {
    fn default() -> Self {
        Self::new()
    }
}

impl MockProcessContext {
    pub fn new() -> Self {
        Self {
            properties: MockPropertyMap::new(),
            controller_services: HashMap::new(),
            attributes: HashMap::new(),
        }
    }
}

impl GetAttribute for MockProcessContext {
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError> {
        Ok(self.attributes.get(name).cloned())
    }
}

impl GetId for MockProcessContext {
    fn get_id(&self) -> Result<String, MinifiError> {
        Ok("mock_flow_file_id".into())
    }
}

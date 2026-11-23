// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::property::PropertySchema;
use crate::api::{GetId, ProcessContext, RawControllerService};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, GetAttribute, MinifiError,
    MockFlowFile, Property, StateAccess,
};
use std::any::Any;
use std::cell::{Cell, RefCell};
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

pub struct MockProcessContext {
    pub properties: MockPropertyMap,
    pub controller_services: HashMap<String, Box<dyn Any>>,
    pub attributes: HashMap<String, String>,
    // Modelled on the C++ `KeyValueStateManager`: a write stays pending until the owning session commits.
    committed_state: RefCell<HashMap<String, String>>,
    pending_state: RefCell<Option<HashMap<String, String>>>,
    transaction_open: Cell<bool>,
    // `set_state` calls, whether or not they reached storage.
    pub state_writes: Cell<usize>,
    // Commits that actually moved a pending write into storage.
    pub storage_writes: Cell<usize>,
}

impl ProcessContext for MockProcessContext {
    type FlowFile = MockFlowFile;

    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
        _flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, _flow_file)
    }

    fn get_raw_controller_service<Cs, P>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static,
        P: PropertySchema + ?Sized,
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

impl StateAccess for MockProcessContext {
    fn get_state(&self) -> Result<HashMap<String, String>, MinifiError> {
        // Reads back the session's own pending write, like `StateManager::getEffectiveState`.
        if let Some(ref pending) = *self.pending_state.borrow() {
            return Ok(pending.clone());
        }
        Ok(self.committed_state.borrow().clone())
    }

    fn set_state(&self, state: &HashMap<String, String>) -> Result<(), MinifiError> {
        self.state_writes.set(self.state_writes.get() + 1);
        // `minifi_process_context_set_state` drops a redundant write, leaving the session clean so its commit
        // performs no storage write.
        if self.get_state()? == *state {
            return Ok(());
        }
        *self.pending_state.borrow_mut() = Some(state.clone());
        if !self.transaction_open.get() {
            // `KeyValueStateManager::set` auto-commits when no transaction is in progress.
            self.commit();
        }
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
            committed_state: RefCell::new(HashMap::new()),
            pending_state: RefCell::new(None),
            transaction_open: Cell::new(false),
            state_writes: Cell::new(0),
            storage_writes: Cell::new(0),
        }
    }

    /// The state as it would survive a restart; excludes pending writes.
    pub fn committed_state(&self) -> HashMap<String, String> {
        self.committed_state.borrow().clone()
    }

    /// As `ProcessSessionImpl`'s constructor does, so a test can run several triggers against one session the way
    /// the event-driven agent does.
    pub fn begin_transaction(&self) {
        self.transaction_open.set(true);
    }

    /// As `ProcessSession::commit`: makes the pending write durable.
    pub fn commit(&self) {
        let pending = self.pending_state.borrow_mut().take();
        if let Some(pending) = pending {
            *self.committed_state.borrow_mut() = pending;
            self.storage_writes.set(self.storage_writes.get() + 1);
        }
        self.transaction_open.set(false);
    }

    /// As `ProcessSession::rollback`: discards the pending write.
    pub fn rollback(&self) {
        self.pending_state.borrow_mut().take();
        self.transaction_open.set(false);
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

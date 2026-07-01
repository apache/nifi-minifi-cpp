use crate::api::{GetId, ProcessContext, RawControllerService};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, GetAttribute, MinifiError,
    MockFlowFile, Property,
};
use std::any::Any;
use std::borrow::Cow;
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
    pub fn get_property(
        &self,
        property: &Property,
        _flow_file: Option<&MockFlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        if let Some(property) = self.properties.get(property.name) {
            Ok(Some(property.clone()))
        } else {
            if let Some(default_val) = property.default_value {
                return Ok(Some(default_val.to_string()));
            }
            match property.is_required {
                true => Err(MinifiError::MissingRequiredProperty(Cow::from(
                    property.name,
                ))),
                false => Ok(None),
            }
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

    fn get_property(
        &self,
        property: &Property,
        _flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, _flow_file)
    }

    fn get_raw_controller_service<Cs>(
        &self,
        property: &Property,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static,
    {
        // Mirror `get_controller_service`: resolve the property to a
        // service name and downcast the registered `Box<dyn Any>`.
        if let Some(service_name) = self.get_property(property, None)? {
            Ok(self
                .controller_services
                .get(&service_name)
                .and_then(|c| c.downcast_ref::<Cs>()))
        } else {
            Ok(None)
        }
    }

    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static,
    {
        if let Some(service_name) = self.get_property(property, None)? {
            Ok(self
                .controller_services
                .get(&service_name)
                .and_then(|c| c.downcast_ref::<Cs>()))
        } else {
            Ok(None)
        }
    }

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi>(
        &self,
        _property: &Property,
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

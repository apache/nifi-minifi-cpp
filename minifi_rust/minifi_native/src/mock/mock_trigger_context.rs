use super::mock_configuration::{MockControllerServices, MockPropertyMap, find_controller_service};
use crate::api::property::PropertySchema;
use crate::api::trigger_context::IoState;
use crate::api::{InputStream, RawControllerService, ScheduleContext, TriggerContext};
use crate::{
    ComponentIdentifier, ControllerServiceApi, EnableControllerService, GetProperty, MinifiError,
    MockFlowFile, MockScheduleContext, Property,
};
use itertools::Itertools;
use std::cell::RefCell;
use std::collections::HashMap;
use std::io::Read;

pub struct TransferredFlowFile {
    pub relationship: String,
    pub flow_file: MockFlowFile,
}

pub struct MockTriggerContext {
    pub properties: MockPropertyMap,
    pub controller_services: MockControllerServices,
    pub input_flow_files: RefCell<Vec<MockFlowFile>>,
    pub transferred_flow_files: RefCell<Vec<TransferredFlowFile>>,
}

impl GetProperty for MockTriggerContext {
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, None)
    }
}

impl ScheduleContext for MockTriggerContext {
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
        Ok(None)
    }
}

impl TriggerContext for MockTriggerContext {
    type FlowFile = MockFlowFile;

    fn get_raw_flow_file_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
        flow_file: &Self::FlowFile,
    ) -> Result<Option<String>, MinifiError> {
        self.properties.get_property(property, Some(flow_file))
    }

    fn report_metrics(&self, _metrics: Vec<(String, f64)>) -> Result<(), MinifiError> {
        Ok(())
    }

    fn create(&self) -> Result<Self::FlowFile, MinifiError> {
        Ok(Self::FlowFile::new())
    }
    fn get(&self) -> Option<Self::FlowFile> {
        self.input_flow_files.borrow_mut().pop()
    }
    fn transfer(&self, flow_file: Self::FlowFile, relationship: &str) -> Result<(), MinifiError> {
        self.transferred_flow_files
            .borrow_mut()
            .push(TransferredFlowFile {
                relationship: relationship.to_string(),
                flow_file,
            });
        Ok(())
    }

    fn remove(&self, _flow_file: Self::FlowFile) -> Result<(), MinifiError> {
        Ok(())
    }

    fn set_attribute(
        &self,
        flow_file: &mut Self::FlowFile,
        attr_key: &str,
        attr_value: &str,
    ) -> Result<(), MinifiError> {
        flow_file
            .attributes
            .insert(attr_key.to_string(), attr_value.to_string());
        Ok(())
    }
    fn get_attribute(&self, flow_file: &Self::FlowFile, attr_key: &str) -> Option<String> {
        flow_file.attributes.get(attr_key).cloned()
    }

    fn for_each_attribute<F: FnMut(&str, &str)>(
        &self,
        flow_file: &Self::FlowFile,
        mut process_attr: F,
    ) -> bool {
        // Sorting for deterministic tests.
        for (attr_key, attr_value) in flow_file.attributes.iter().sorted_by_key(|x| x.0) {
            process_attr(attr_key, attr_value);
        }
        true
    }

    fn write(&self, flow_file: &Self::FlowFile, data: &[u8]) -> Result<(), MinifiError> {
        *flow_file.content.borrow_mut() = data.to_vec();
        Ok(())
    }

    fn write_from_stream<'a>(
        &self,
        flow_file: &Self::FlowFile,
        mut stream: Box<dyn Read + 'a>,
    ) -> Result<(), MinifiError> {
        let mut buf = flow_file.content.borrow_mut();
        buf.clear();
        stream
            .read_to_end(&mut buf)
            .expect("Mock data should be readable");
        Ok(())
    }

    fn write_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(
            &mut dyn crate::api::trigger_context::OutputStream,
        ) -> Result<(R, IoState), MinifiError>,
    {
        let mut new_content: Vec<u8> = Vec::new();
        let mut cursor = std::io::Cursor::new(&mut new_content);
        let (r, state) = callback(&mut cursor)?;
        if state == IoState::Ok {
            *flow_file.content.borrow_mut() = new_content;
        }
        Ok(r)
    }

    fn read(&self, flow_file: &Self::FlowFile) -> Option<Vec<u8>> {
        Some(flow_file.content.borrow().clone())
    }

    fn read_stream<F, R>(&self, flow_file: &Self::FlowFile, callback: F) -> Result<R, MinifiError>
    where
        F: FnOnce(&mut dyn InputStream) -> Result<R, MinifiError>,
    {
        let bytes = flow_file.content.borrow().clone();
        let mut cursor = std::io::Cursor::new(bytes);
        callback(&mut cursor)
    }

    fn get_flow_file_id(&self, flow_file: &Self::FlowFile) -> Result<String, MinifiError> {
        Ok(flow_file.id.clone())
    }
}

impl Default for MockTriggerContext {
    fn default() -> Self {
        Self::new()
    }
}

impl MockTriggerContext {
    pub fn new() -> Self {
        Self {
            properties: MockPropertyMap::new(),
            controller_services: HashMap::new(),
            input_flow_files: RefCell::new(Vec::new()),
            transferred_flow_files: RefCell::new(Vec::new()),
        }
    }

    pub fn num_of_transferred_flow_files(&self) -> usize {
        self.transferred_flow_files.borrow().len()
    }
}

impl From<MockScheduleContext> for MockTriggerContext {
    fn from(schedule_context: MockScheduleContext) -> Self {
        Self {
            properties: schedule_context.properties,
            controller_services: schedule_context.controller_services,
            ..Self::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::api::trigger_context::IoState;

    #[test]
    fn test_write_from_stream_replaces_content() {
        let context = MockTriggerContext::new();
        let flow_file = MockFlowFile::new();
        *flow_file.content.borrow_mut() = b"Hello, World!".to_vec();

        let source: &[u8] = b"Hi";
        context
            .write_from_stream(&flow_file, Box::new(source))
            .expect("write_from_stream should succeed");

        assert_eq!(*flow_file.content.borrow(), b"Hi");
    }

    #[test]
    fn test_write_stream_replaces_content() {
        let context = MockTriggerContext::new();
        let flow_file = MockFlowFile::new();
        // Pre-populate the flow file with longer content
        *flow_file.content.borrow_mut() = b"Hello, World!".to_vec();

        let res: Result<(), MinifiError> = context.write_stream(&flow_file, |stream| {
            stream.write_all(b"Hi")?;
            Ok(((), IoState::Ok))
        });

        assert!(res.is_ok());
        // Old trailing bytes must not survive
        assert_eq!(*flow_file.content.borrow(), b"Hi");
    }
}

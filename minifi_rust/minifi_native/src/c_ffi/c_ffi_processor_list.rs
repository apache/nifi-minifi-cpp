use crate::c_ffi::RawRegisterableProcessor;
use crate::c_ffi::c_ffi_processor_definition::DynRawProcessorDefinition;
use minifi_native_sys::MinifiProcessorClassDefinition;

pub struct CffiProcessorList {
    processor_definitions: Vec<Box<dyn DynRawProcessorDefinition>>,
    minifi_processor_class_description_list: Vec<MinifiProcessorClassDefinition>,
}

impl CffiProcessorList {
    pub fn new() -> Self {
        Self {
            processor_definitions: Vec::new(),
            minifi_processor_class_description_list: Vec::new(),
        }
    }

    pub fn add<T: RawRegisterableProcessor>(&mut self) {
        self.add_processor_definition(T::get_definition())
    }

    pub fn add_processor_definition(
        &mut self,
        processor_definition: Box<dyn DynRawProcessorDefinition>,
    ) {
        unsafe {
            self.processor_definitions.push(processor_definition);
            self.minifi_processor_class_description_list.push(
                self.processor_definitions
                    .last()
                    .unwrap()
                    .class_description()
                    .as_raw(),
            );
        }
    }

    pub fn get_processor_count(&self) -> usize {
        assert_eq!(
            self.processor_definitions.len(),
            self.minifi_processor_class_description_list.len()
        );
        self.minifi_processor_class_description_list.len()
    }

    pub unsafe fn get_processor_ptr(&self) -> *const MinifiProcessorClassDefinition {
        self.minifi_processor_class_description_list.as_ptr()
    }
}

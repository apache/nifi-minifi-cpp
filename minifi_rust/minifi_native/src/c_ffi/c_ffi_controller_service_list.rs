use crate::c_ffi::RegisterableControllerService;
use crate::c_ffi::c_ffi_controller_service_definition::DynRawControllerServiceDefinition;
use minifi_native_sys::MinifiControllerServiceClassDefinition;

pub struct CffiControllerServiceList {
    controller_service_definitions: Vec<Box<dyn DynRawControllerServiceDefinition>>,
    minifi_controller_service_class_description_list: Vec<MinifiControllerServiceClassDefinition>,
}

impl CffiControllerServiceList {
    pub fn new() -> Self {
        Self {
            controller_service_definitions: Vec::new(),
            minifi_controller_service_class_description_list: Vec::new(),
        }
    }

    pub fn add<T: RegisterableControllerService>(&mut self) {
        self.add_controller_service_definition(T::get_definition())
    }

    pub fn add_controller_service_definition(
        &mut self,
        processor_definition: Box<dyn DynRawControllerServiceDefinition>,
    ) {
        unsafe {
            self.controller_service_definitions
                .push(processor_definition);
            self.minifi_controller_service_class_description_list.push(
                self.controller_service_definitions
                    .last()
                    .unwrap()
                    .class_description()
                    .as_raw(),
            );
        }
    }

    pub fn get_controller_service_count(&self) -> usize {
        assert_eq!(
            self.controller_service_definitions.len(),
            self.minifi_controller_service_class_description_list.len()
        );
        self.minifi_controller_service_class_description_list.len()
    }

    pub unsafe fn get_controller_service_ptr(
        &self,
    ) -> *const MinifiControllerServiceClassDefinition {
        self.minifi_controller_service_class_description_list
            .as_ptr()
    }
}

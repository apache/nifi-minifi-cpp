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

use crate::c_ffi::RegisterableControllerService;
use crate::c_ffi::c_ffi_controller_service_definition::DynRawControllerServiceDefinition;
use minifi_native_sys::minifi_controller_service_definition;

pub struct CffiControllerServiceList {
    controller_service_definitions: Vec<Box<dyn DynRawControllerServiceDefinition>>,
    minifi_controller_service_class_description_list: Vec<minifi_controller_service_definition>,
}

impl Default for CffiControllerServiceList {
    fn default() -> Self {
        Self::new()
    }
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

    /// # Safety
    ///
    /// The returned *minifi_controller_service_definition only valid until self lives
    /// TODO(mzink) maybe some lifetimes?
    pub unsafe fn get_controller_service_ptr(&self) -> *const minifi_controller_service_definition {
        self.minifi_controller_service_class_description_list
            .as_ptr()
    }
}

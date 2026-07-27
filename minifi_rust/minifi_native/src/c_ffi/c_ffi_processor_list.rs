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

use crate::c_ffi::RawRegisterableProcessor;
use crate::c_ffi::c_ffi_processor_definition::DynRawProcessorDefinition;
use minifi_native_sys::minifi_processor_definition;

pub struct CffiProcessorList {
    processor_definitions: Vec<Box<dyn DynRawProcessorDefinition>>,
    minifi_processor_class_description_list: Vec<minifi_processor_definition>,
}

impl Default for CffiProcessorList {
    fn default() -> Self {
        Self::new()
    }
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

    /// # Safety
    ///
    /// The returned *minifi_processor_definition only valid until self lives
    /// TODO(mzink) maybe some lifetimes?
    pub unsafe fn get_processor_ptr(&self) -> *const minifi_processor_definition {
        self.minifi_processor_class_description_list.as_ptr()
    }
}

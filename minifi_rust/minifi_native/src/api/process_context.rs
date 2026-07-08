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

use crate::api::RawControllerService;
use crate::api::component_definition_traits::ComponentIdentifier;
use crate::api::flow_file::FlowFile;
use crate::api::property::GetControllerService;
use crate::{
    ControllerServiceApi, ControllerServiceDefinition, EnableControllerService, GetProperty,
    MinifiError, Property,
};

pub trait ProcessContext {
    type FlowFile: FlowFile;

    fn get_property(
        &self,
        property: &Property,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError>;

    fn get_raw_controller_service<Cs>(
        &self,
        property: &Property,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static;

    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static;

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi>(
        &self,
        property: &Property,
    ) -> Result<Option<Box<&Trait>>, MinifiError>;

    fn report_metrics(&self, metrics: Vec<(String, f64)>) -> Result<(), MinifiError>;
}

impl<S> GetProperty for S
where
    S: ProcessContext,
{
    fn get_raw_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        self.get_property(property, None)
    }
}

impl<S> GetControllerService for S
where
    S: ProcessContext,
{
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + ControllerServiceDefinition + 'static,
    {
        ProcessContext::get_controller_service::<Cs>(self, property)
    }
}

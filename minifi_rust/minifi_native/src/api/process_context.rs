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
use crate::api::property::{ControllerServiceValue, GetControllerService, PropertySchema};
use crate::{ControllerServiceApi, EnableControllerService, GetProperty, MinifiError, Property};

pub trait ProcessContext {
    type FlowFile: FlowFile;

    fn get_raw_property<K: PropertySchema + ?Sized>(
        &self,
        property: &Property<K>,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError>;

    fn get_raw_controller_service<Cs, K>(
        &self,
        property: &Property<K>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: RawControllerService + ComponentIdentifier + 'static,
        K: PropertySchema + ?Sized;

    fn get_controller_service<Cs>(
        &self,
        property: &Property<Cs>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static;

    fn get_controller_service_api<Trait: ?Sized + ControllerServiceApi + PropertySchema>(
        &self,
        property: &Property<Trait>,
    ) -> Result<Option<Box<&Trait>>, MinifiError>;

    fn report_metrics(&self, metrics: Vec<(String, f64)>) -> Result<(), MinifiError>;
}

impl<S> GetProperty for S
where
    S: ProcessContext,
{
    fn get_raw_property<K: PropertySchema + ?Sized>(
        &self,
        property: &Property<K>,
    ) -> Result<Option<String>, MinifiError> {
        self.get_raw_property(property, None)
    }
}

impl<S> GetControllerService for S
where
    S: ProcessContext,
{
    fn get_controller_service<K>(
        &self,
        property: &Property<K>,
    ) -> Result<K::Output<'_>, MinifiError>
    where
        K: ControllerServiceValue + ?Sized,
    {
        let cs_property = property.with_marker::<K::Cs>();
        let service = ProcessContext::get_controller_service::<K::Cs>(self, &cs_property)?;
        K::from_service(service, property.name)
    }
}

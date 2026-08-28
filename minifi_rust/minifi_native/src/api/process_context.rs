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

    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
        flow_file: Option<&Self::FlowFile>,
    ) -> Result<Option<String>, MinifiError>;

    /// Returns the RawControllerService (ControllerService wrapper whose lifetime is managed by the agent)
    fn get_raw_controller_service<RawCs, P>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<&RawCs>, MinifiError>
    where
        RawCs: RawControllerService + ComponentIdentifier + 'static,
        P: PropertySchema + ?Sized;

    /// Returns the enabled ControllerService (managed by RawControllerService)
    fn get_controller_service<Cs>(
        &self,
        property: &Property<Cs>,
    ) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + PropertySchema + 'static;

    /// Returns the enabled type erased ControllerService via the registered ControllerServiceApi
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
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        self.get_raw_property(property, None)
    }
}

impl<S> GetControllerService for S
where
    S: ProcessContext,
{
    fn get_controller_service<P>(
        &self,
        property: &Property<P>,
    ) -> Result<P::Output<'_>, MinifiError>
    where
        P: ControllerServiceValue + ?Sized,
    {
        let cs_property = property.with_marker::<P::Cs>();
        let service = ProcessContext::get_controller_service::<P::Cs>(self, &cs_property)?;
        P::from_service(service, property.name)
    }
}

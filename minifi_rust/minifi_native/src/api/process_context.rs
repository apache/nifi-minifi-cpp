// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

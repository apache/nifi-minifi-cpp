// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::attribute::GetAttribute;
use crate::api::flow_file::GetId;
use crate::api::property::{
    ControllerServiceValue, GetControllerService, GetProperty, PropertySchema,
};
use crate::{MinifiError, Property, TriggerContext};

pub struct FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    context: &'a TC,
    flow_file: Option<&'a TC::FlowFile>,
}

impl<'a, TC> FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    pub(crate) fn new(context: &'a TC, flow_file: Option<&'a TC::FlowFile>) -> Self {
        Self { context, flow_file }
    }
}
impl<'a, TC> GetProperty for FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        match self.flow_file {
            Some(flow_file) => self.context.get_raw_flow_file_property(property, flow_file),
            None => self.context.get_raw_property(property),
        }
    }
}

impl<'a, TC> GetControllerService for FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    fn get_controller_service<P>(
        &self,
        property: &Property<P>,
    ) -> Result<P::Output<'_>, MinifiError>
    where
        P: ControllerServiceValue + ?Sized,
    {
        GetControllerService::get_controller_service(self.context, property)
    }
}

impl<'a, TC> GetAttribute for FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError> {
        if let Some(ff) = self.flow_file {
            Ok(self.context.get_attribute(ff, name))
        } else {
            Ok(None)
        }
    }
}

impl<'a, TC> GetId for FlowFileContext<'a, TC>
where
    TC: TriggerContext,
{
    fn get_id(&self) -> Result<String, MinifiError> {
        if let Some(ff) = self.flow_file {
            self.context.get_flow_file_id(ff)
        } else {
            Err(MinifiError::MissingFlowFileError)
        }
    }
}

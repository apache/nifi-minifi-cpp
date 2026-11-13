// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::attribute::GetAttribute;
use crate::api::flow_file::GetId;
use crate::api::property::{
    ControllerServiceValue, GetControllerService, GetProperty, PropertySchema,
};
use crate::{MinifiError, ProcessContext, ProcessSession, Property};

pub struct ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    context: &'a PC,
    session: &'a PS,
    flow_file: Option<&'a PC::FlowFile>,
}

impl<'a, PC, PS> ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    pub(crate) fn new(
        context: &'a PC,
        session: &'a PS,
        flow_file: Option<&'a PC::FlowFile>,
    ) -> Self {
        Self {
            context,
            session,
            flow_file,
        }
    }
}
impl<'a, PC, PS> GetProperty for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_raw_property<P: PropertySchema + ?Sized>(
        &self,
        property: &Property<P>,
    ) -> Result<Option<String>, MinifiError> {
        self.context.get_raw_property(property, self.flow_file)
    }
}

impl<'a, PC, PS> GetControllerService for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
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

impl<'a, PC, PS> GetAttribute for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_attribute(&self, name: &str) -> Result<Option<String>, MinifiError> {
        if let Some(ff) = self.flow_file {
            Ok(self.session.get_attribute(ff, name))
        } else {
            Ok(None)
        }
    }
}

impl<'a, PC, PS> GetId for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_id(&self) -> Result<String, MinifiError> {
        if let Some(ff) = self.flow_file {
            self.session.get_flow_file_id(ff)
        } else {
            Err(MinifiError::MissingFlowFileError)
        }
    }
}

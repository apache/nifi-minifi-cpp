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

use crate::api::attribute::GetAttribute;
use crate::api::flow_file::GetId;
use crate::api::property::{GetControllerService, GetProperty};
use crate::{
    ComponentIdentifier, EnableControllerService, MinifiError, ProcessContext, ProcessSession,
    Property,
};

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
    fn get_raw_property(&self, property: &Property) -> Result<Option<String>, MinifiError> {
        self.context.get_property(property, self.flow_file)
    }
}

impl<'a, PC, PS> GetControllerService for ContextSessionFlowFileBundle<'a, PC, PS>
where
    PC: ProcessContext,
    PS: ProcessSession<FlowFile = PC::FlowFile>,
{
    fn get_controller_service<Cs>(&self, property: &Property) -> Result<Option<&Cs>, MinifiError>
    where
        Cs: EnableControllerService + ComponentIdentifier + 'static,
    {
        self.context.get_controller_service(property)
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

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

// Simple test processor that uses a controller service
mod properties;

use crate::processors::lorem_ipsum_cs_user::properties::{
    CONTROLLER_SERVICE, DUMMY_CONTROLLER_SERVICE,
};
use crate::processors::lorem_ipsum_cs_user::relationships::SUCCESS;
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    Content, FlowFileSource, GeneratedFlowFile, GetControllerService, GetProperty, Logger,
    MinifiError, ProcessError, Schedule, trace,
};
use strum_macros::{Display, EnumString, IntoStaticStr, VariantNames};

#[derive(
    Debug, Clone, Copy, PartialEq, Display, EnumString, VariantNames, IntoStaticStr, PropertyType,
)]
#[strum(serialize_all = "PascalCase", const_into_str)]
enum WriteMethod {
    Buffer,
    Stream,
}

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct LoremIpsumCSUser {
    write_method: WriteMethod,
}

impl Schedule for LoremIpsumCSUser {
    fn schedule<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let write_method = context.get_property(&properties::WRITE_METHOD)?;
        Ok(Self { write_method })
    }
}

impl FlowFileSource for LoremIpsumCSUser {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, ProcessError> {
        trace!(logger, "generate call {:?}", self);
        let dummy_controller_service = context.get_controller_service(&DUMMY_CONTROLLER_SERVICE)?;
        trace!(
            logger,
            "optional dummy controller service: {:?}", dummy_controller_service
        );
        let controller_service = context.get_controller_service(&CONTROLLER_SERVICE)?;
        match self.write_method {
            WriteMethod::Buffer => {
                let generated_flow_file = GeneratedFlowFile::new(
                    &SUCCESS,
                    Some(Content::from(controller_service.data.clone())),
                );
                Ok(vec![generated_flow_file])
            }
            WriteMethod::Stream => {
                let reader = controller_service.data.as_bytes();
                let content = Content::Stream(Box::new(reader));
                let generated_flow_file = GeneratedFlowFile::new(&SUCCESS, Some(content));
                Ok(vec![generated_flow_file])
            }
        }
    }
}

pub(crate) mod processor_definition;

mod relationships;
#[cfg(test)]
mod tests;

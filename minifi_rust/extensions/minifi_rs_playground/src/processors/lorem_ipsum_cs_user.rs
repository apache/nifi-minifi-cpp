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

use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::lorem_ipsum_cs_user::properties::CONTROLLER_SERVICE;
use crate::processors::lorem_ipsum_cs_user::relationships::SUCCESS;
use minifi_native::macros::{ComponentIdentifier, PropertyType};
use minifi_native::{
    Content, FlowFileSource, GeneratedFlowFile, GetControllerService, GetProperty, Logger,
    MinifiError, Schedule, trace,
};
use std::collections::HashMap;
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
        let write_method = context.get_req_property::<WriteMethod>(&properties::WRITE_METHOD)?;
        Ok(Self { write_method })
    }
}

impl FlowFileSource for LoremIpsumCSUser {
    fn generate<'a, Context: GetProperty + GetControllerService, LoggerImpl: Logger>(
        &self,
        context: &'a mut Context,
        logger: &LoggerImpl,
    ) -> Result<Vec<GeneratedFlowFile<'a>>, MinifiError> {
        trace!(logger, "generate call {:?}", self);
        let controller_service = context
            .get_controller_service::<LoremIpsumControllerService>(&CONTROLLER_SERVICE)?
            .ok_or(MinifiError::missing_required_property(
                "A valid usable controller service is required",
            ))?;
        match self.write_method {
            WriteMethod::Buffer => {
                let generated_flow_file = GeneratedFlowFile::new(
                    &SUCCESS,
                    Some(Content::from(controller_service.data.clone())),
                    HashMap::new(),
                );
                Ok(vec![generated_flow_file])
            }
            WriteMethod::Stream => {
                let reader = controller_service.data.as_bytes();
                let content = Content::Stream(Box::new(reader));
                let generated_flow_file =
                    GeneratedFlowFile::new(&SUCCESS, Some(content), HashMap::new());
                Ok(vec![generated_flow_file])
            }
        }
    }
}

pub(crate) mod processor_definition;

mod relationships;
#[cfg(test)]
mod tests;

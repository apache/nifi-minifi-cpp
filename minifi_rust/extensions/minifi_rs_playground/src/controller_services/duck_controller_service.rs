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

use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::ControllerServiceApi;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, ProvidedInterface, create_provided_interface,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DuckControllerRs {}

impl NumberOfLegsControllerApi for DuckControllerRs {
    fn number_of_legs(&self) -> u8 {
        2
    }
}

impl CanFlyControllerApi for DuckControllerRs {
    fn can_fly(&self) -> bool {
        true
    }
}

impl EnableControllerService for DuckControllerRs {
    fn enable<Ctx: GetProperty, L: Logger>(_context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl ControllerServiceDefinition for DuckControllerRs {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: DuckControllerRs";
    const PROPERTIES: &'static [Property] = &[];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}

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
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{ControllerServiceApi, property_constraint};
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, ProvidedInterface, create_provided_interface,
};

pub(crate) const HAS_JETPACK: Property = Property {
    name: "Has Jetpack",
    description: "Whether or not the dog has a jetpack",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("false"),
    constraints: property_constraint::<bool>(),
};

pub(crate) const EXTRA_INFO: Property = Property {
    name: "Extra information",
    description: "We need this to verify the casting was done correctly",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    constraints: None,
};

#[allow(dead_code)] // extra_info is only used by {:?}
#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DogControllerRs {
    has_jetpack: bool,
    extra_info: String,
}

impl NumberOfLegsControllerApi for DogControllerRs {
    fn number_of_legs(&self) -> u8 {
        4
    }
}

impl CanFlyControllerApi for DogControllerRs {
    fn can_fly(&self) -> bool {
        self.has_jetpack
    }
}

impl EnableControllerService for DogControllerRs {
    fn enable<Ctx: GetProperty, L: Logger>(context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let has_jetpack = context.get_req_property::<bool>(&HAS_JETPACK)?;
        let extra_info = context
            .get_property::<String>(&EXTRA_INFO)?
            .unwrap_or("".into());

        Ok(Self {
            has_jetpack,
            extra_info,
        })
    }
}

impl ControllerServiceDefinition for DogControllerRs {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: DogControllerRs";
    const PROPERTIES: &'static [Property] = &[HAS_JETPACK, EXTRA_INFO];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}

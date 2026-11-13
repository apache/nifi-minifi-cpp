// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::controller_services::animal_controller_apis::{
    CanFlyControllerApi, NumberOfLegsControllerApi,
};
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceApi, ControllerServiceDefinition, EnableControllerService, GetProperty,
    Logger, MinifiError, Property, PropertyDefinition, ProvidedInterface,
    create_provided_interface, property_definitions,
};

pub(crate) const HAS_JETPACK: Property<bool> =
    Property::new("Has Jetpack", "Whether or not the dog has a jetpack").with_default("false");

pub(crate) const EXTRA_INFO: Property<Option<String>> = Property::new(
    "Extra information",
    "We need this to verify the casting was done correctly",
);

#[allow(dead_code)] // extra_info is only used by {:?}
#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DogControllerRs {
    has_jetpack: bool,
    extra_info: Option<String>,
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
        let has_jetpack = context.get_property(&HAS_JETPACK)?;
        let extra_info = context.get_property(&EXTRA_INFO)?;

        Ok(Self {
            has_jetpack,
            extra_info,
        })
    }
}

impl ControllerServiceDefinition for DogControllerRs {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: DogControllerRs";
    const PROPERTIES: &'static [PropertyDefinition] =
        property_definitions![HAS_JETPACK, EXTRA_INFO];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[
        create_provided_interface!(dyn CanFlyControllerApi),
        create_provided_interface!(dyn NumberOfLegsControllerApi),
    ];
}

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use lipsum::lipsum;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, PropertyDefinition, ProvidedInterface, property_definitions,
};

const LENGTH: Property<usize> =
    Property::new("Length", "How many words to generate").with_default("25");

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct LoremIpsumControllerService {
    pub data: String,
}

impl EnableControllerService for LoremIpsumControllerService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let data = lipsum(context.get_property(&LENGTH)?);
        Ok(Self { data })
    }
}

impl ControllerServiceDefinition for LoremIpsumControllerService {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: Holds generated lorem ipsum";
    const PROPERTIES: &'static [PropertyDefinition] = property_definitions![LENGTH];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}

// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    PropertyDefinition, ProvidedInterface,
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct DummyControllerService {}

impl EnableControllerService for DummyControllerService {
    fn enable<Ctx: GetProperty, L: Logger>(_context: &Ctx, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        Ok(Self {})
    }
}

impl ControllerServiceDefinition for DummyControllerService {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: Dummy Controller Service";
    const PROPERTIES: &'static [PropertyDefinition] = &[];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}

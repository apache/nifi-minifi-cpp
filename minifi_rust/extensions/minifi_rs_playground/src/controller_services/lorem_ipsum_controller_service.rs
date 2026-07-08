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

use lipsum::lipsum;
use minifi_native::macros::ComponentIdentifier;
use minifi_native::{
    ControllerServiceDefinition, EnableControllerService, GetProperty, Logger, MinifiError,
    Property, ProvidedInterface, property_constraint,
};

const LENGTH: Property = Property {
    name: "Length",
    description: "How many words to generate",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some("25"),
    constraints: property_constraint::<u64>(),
};

#[derive(Debug, ComponentIdentifier)]
pub(crate) struct LoremIpsumControllerService {
    pub data: String,
}

impl EnableControllerService for LoremIpsumControllerService {
    fn enable<P: GetProperty, L: Logger>(context: &P, _logger: &L) -> Result<Self, MinifiError>
    where
        Self: Sized,
    {
        let data = lipsum(context.get_req_property::<usize>(&LENGTH)?);
        Ok(Self { data })
    }
}

impl ControllerServiceDefinition for LoremIpsumControllerService {
    const DESCRIPTION: &'static str = "RUST TEST CONTROLLER SERVICE: Holds generated lorem ipsum";
    const PROPERTIES: &'static [Property] = &[LENGTH];
    const PROVIDED_APIS: &'static [ProvidedInterface<Self>] = &[];
}

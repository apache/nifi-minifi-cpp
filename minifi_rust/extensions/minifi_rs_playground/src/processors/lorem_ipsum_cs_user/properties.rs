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

use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use minifi_native::ComponentIdentifier;
use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const CONTROLLER_SERVICE: Property = Property {
    name: "Lorem Ipsum Controller Service",
    description: "Name of the lorem ipsum controller service",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: Some(LoremIpsumControllerService::CLASS_NAME),
};

pub(crate) const WRITE_METHOD: Property = Property {
    name: "Write Method",
    description: "Which API to test",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(super::WriteMethod::Buffer.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: super::WriteMethod::VARIANTS,
    allowed_type: None,
};

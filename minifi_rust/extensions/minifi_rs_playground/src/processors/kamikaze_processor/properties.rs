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

use crate::processors::kamikaze_processor::KamikazeBehaviour;
use minifi_native::{Property, StandardPropertyValidator};
use strum::VariantNames;

pub(crate) const SCHEDULE_BEHAVIOUR: Property = Property {
    name: "Schedule Behaviour",
    description: "What to do during the on_schedule method",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(KamikazeBehaviour::ReturnOk.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: KamikazeBehaviour::VARIANTS,
    allowed_type: None,
};

pub(crate) const TRIGGER_BEHAVIOUR: Property = Property {
    name: "Trigger Behaviour",
    description: "What to do during the trigger method",
    is_required: true,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: Some(KamikazeBehaviour::ReturnOk.into_str()),
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: KamikazeBehaviour::VARIANTS,
    allowed_type: None,
};

pub(crate) const NOT_REGISTERED_PROPERTY: Property = Property {
    name: "Kamikaze Processor Property",
    description: "Property purposely left out of Processor description",
    is_required: false,
    is_sensitive: false,
    supports_expr_lang: false,
    default_value: None,
    validator: StandardPropertyValidator::AlwaysValidValidator,
    allowed_values: &[],
    allowed_type: None,
};

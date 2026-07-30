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
use crate::processors::kamikaze_processor::KamikazeBehaviour;
use minifi_native::Property;

pub(crate) const SCHEDULE_BEHAVIOUR: Property<KamikazeBehaviour> = Property::new(
    "Schedule Behaviour",
    "What to do during the on_schedule method",
)
.with_default(KamikazeBehaviour::ReturnOk.into_str());

pub(crate) const TRIGGER_BEHAVIOUR: Property<KamikazeBehaviour> =
    Property::new("Trigger Behaviour", "What to do during the trigger method")
        .with_default(KamikazeBehaviour::ReturnOk.into_str());

pub(crate) const NOT_REGISTERED_PROPERTY: Property<Option<String>> = Property::new(
    "Kamikaze Processor Property",
    "Property purposely left out of Processor description",
);

pub(crate) const UNREGISTERED_CONTROLLER_SERVICE: Property<LoremIpsumControllerService> =
    Property::new(
        "Kamikaze Processor Property",
        "Property purposely left out of Processor description",
    );

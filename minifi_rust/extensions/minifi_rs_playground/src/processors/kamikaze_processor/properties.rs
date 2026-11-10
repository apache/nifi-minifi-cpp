// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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

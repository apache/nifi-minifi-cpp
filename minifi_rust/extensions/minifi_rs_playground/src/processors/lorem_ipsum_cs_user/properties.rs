// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::controller_services::dummy_controller_service::DummyControllerService;
use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::lorem_ipsum_cs_user::WriteMethod;
use minifi_native::Property;

pub(crate) const CONTROLLER_SERVICE: Property<LoremIpsumControllerService> = Property::new(
    "Lorem Ipsum Controller Service",
    "Name of the lorem ipsum controller service",
);

pub(crate) const DUMMY_CONTROLLER_SERVICE: Property<Option<DummyControllerService>> = Property::new(
    "Dummy Controller Service",
    "Optional dummy controller service",
);

pub(crate) const WRITE_METHOD: Property<WriteMethod> =
    Property::new("Write Method", "Which API to test").with_default(WriteMethod::Buffer.into_str());

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

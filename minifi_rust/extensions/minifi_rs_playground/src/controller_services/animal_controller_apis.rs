// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use minifi_native::macros::controller_service_api;
use std::fmt::Debug;

#[controller_service_api]
pub trait NumberOfLegsControllerApi: Debug {
    fn number_of_legs(&self) -> u8;
}

#[controller_service_api]
pub trait CanFlyControllerApi: Debug {
    fn can_fly(&self) -> bool;
}

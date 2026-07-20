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

use crate::processors::lorem_ipsum_cs_user::LoremIpsumCSUser;
use minifi_native::{ComponentIdentifier, MockLogger, MockProcessContext, Schedule};

#[test]
fn test_ids() {
    assert_eq!(
        LoremIpsumCSUser::CLASS_NAME,
        "minifi_rs_playground::processors::lorem_ipsum_cs_user::LoremIpsumCSUser"
    );
    assert_eq!(LoremIpsumCSUser::GROUP_NAME, "minifi_rs_playground");
    assert_eq!(LoremIpsumCSUser::VERSION, "0.1.0");
}

#[test]
fn schedules_with_controller() {
    let context = MockProcessContext::new();
    let schedule_result = LoremIpsumCSUser::schedule(&context, &MockLogger::new());
    assert!(schedule_result.is_ok());
}

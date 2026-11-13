// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::controller_services::lorem_ipsum_controller_service::LoremIpsumControllerService;
use crate::processors::lorem_ipsum_cs_user::LoremIpsumCSUser;
use minifi_native::{
    ComponentIdentifier, EnableControllerService, FlowFileSource, MockLogger, MockProcessContext,
    Schedule,
};

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
fn lorem_ipsum_trigger() {
    let mut context = MockProcessContext::new();
    let logger = MockLogger::new();
    let lorem_ipsum_user = LoremIpsumCSUser::schedule(&context, &logger).expect("should schedule");
    let lorem_ipsum_cs = LoremIpsumControllerService::enable(&context, &logger).expect("");
    context
        .controller_services
        .insert("service".into(), Box::new(lorem_ipsum_cs));
    context
        .properties
        .insert("Lorem Ipsum Controller Service", "service");
    let res = lorem_ipsum_user.generate(&mut context, &logger).unwrap();
    assert_eq!(res.len(), 1);
}

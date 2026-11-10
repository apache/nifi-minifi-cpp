// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

use crate::api::FlowFile;
use minifi_native_sys::minifi_flow_file;

pub struct CffiFlowFile<'a> {
    ptr: *mut minifi_flow_file,
    _lifetime: std::marker::PhantomData<&'a ()>,
}

impl CffiFlowFile<'_> {
    pub(crate) fn new(ptr: *mut minifi_flow_file) -> Self {
        Self {
            ptr,
            _lifetime: std::marker::PhantomData,
        }
    }

    pub(crate) fn get_ptr(&self) -> *mut minifi_flow_file {
        self.ptr
    }
}

impl FlowFile for CffiFlowFile<'_> {}

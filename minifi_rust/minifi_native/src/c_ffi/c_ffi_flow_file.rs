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

use crate::api::FlowFile;
use minifi_native_sys::{minifi_flow_file, minifi_stashed_flow_file};

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

/// A handle to a flow file that has been stashed with a session and can be kept
/// across `onTrigger` invocations (see [`ProcessSession::stash`]).
///
/// Unlike [`CffiFlowFile`] it carries no session lifetime, so it is `'static`.
/// The stashed flow file itself is owned by the processor/session on the minifi
/// side; this is only a token used to [`unstash`](crate::ProcessSession::unstash)
/// it again. There is deliberately no explicit free: a token dropped without
/// being unstashed simply leaves the stashed flow file in the processor, which
/// minifi reclaims when the processor is torn down.
///
/// [`ProcessSession::stash`]: crate::ProcessSession::stash
pub struct CffiStashedFlowFile {
    ptr: *mut minifi_stashed_flow_file,
}

// SAFETY: the handle is only ever touched from the single processor instance
// that stashed it; the stashed flow file lives on the minifi side, not behind
// this pointer.
unsafe impl Send for CffiStashedFlowFile {}

impl CffiStashedFlowFile {
    pub(crate) fn new(ptr: *mut minifi_stashed_flow_file) -> Self {
        Self { ptr }
    }

    pub(crate) fn get_ptr(&self) -> *mut minifi_stashed_flow_file {
        self.ptr
    }
}

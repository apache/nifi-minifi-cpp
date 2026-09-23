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

use crate::{
    FlowFileStreamTransform, FlowFileTransform, MinifiError, Relationship, StateAccess,
    StateSchema, TransformError, TransformStreamResult, TransformedFlowFile,
};

/// Assert that the error of a [`FlowFileTransform::transform`] call ends up routed to
/// `expected_relationship`.
///
/// Resolves the error the way the processor wrapper does, so a bubbled error counts as a route
/// to `T`'s [`FlowFileTransform::ERROR_RELATIONSHIP`] and a rollback fails the assertion.
pub fn assert_routed_to<T: FlowFileTransform>(
    res: Result<TransformedFlowFile<'_>, TransformError>,
    expected_relationship: &Relationship,
) {
    assert_error_routed_to(
        res.map(|_| ()),
        T::ERROR_RELATIONSHIP,
        expected_relationship,
    );
}

/// [`assert_routed_to`] for [`FlowFileStreamTransform::transform`] results.
pub fn assert_stream_routed_to<T: FlowFileStreamTransform>(
    res: Result<TransformStreamResult, TransformError>,
    expected_relationship: &Relationship,
) {
    assert_error_routed_to(
        res.map(|_| ()),
        T::ERROR_RELATIONSHIP,
        expected_relationship,
    );
}

/// Store `component`'s `#[persisted]` fields into `state` the way the framework does after a trigger. The fields
/// own the whole map, so anything already in `state` is replaced.
pub fn persist_state<T: StateSchema, S: StateAccess>(
    component: &T,
    state: &S,
) -> Result<(), MinifiError> {
    let mut stored = std::collections::HashMap::new();
    component.store_to_state(&mut stored)?;
    state.set_state(&stored)
}

/// Restore `component`'s `#[persisted]` fields from `state`. Pair with [`persist_state`] to simulate a restart.
pub fn restore_state<T: StateSchema, S: StateAccess>(
    component: &mut T,
    state: &S,
) -> Result<(), MinifiError> {
    let stored = state.get_state()?;
    component.load_from_state(&stored)
}

fn assert_error_routed_to(
    res: Result<(), TransformError>,
    error_relationship: &Relationship,
    expected_relationship: &Relationship,
) {
    match res {
        Err(err) => match err.into_route(error_relationship) {
            Ok(route) => assert_eq!(route.relationship, expected_relationship.name),
            Err(rollback) => {
                panic!("expected route to '{expected_relationship}', got rollback: {rollback:?}")
            }
        },
        Ok(_) => panic!("expected route to '{expected_relationship}', got Ok"),
    }
}

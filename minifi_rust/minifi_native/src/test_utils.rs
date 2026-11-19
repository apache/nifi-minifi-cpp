use crate::{
    FlowFileStreamTransform, FlowFileTransform, Relationship, TransformError,
    TransformStreamResult, TransformedFlowFile,
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
